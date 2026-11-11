#include "mqtt_backend_esp32.h"

#ifdef USE_MQTT
#ifdef USE_ESP32

#include <algorithm>
#include <string>
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#ifdef USE_MQTT_CERT_BUNDLE
#include "esp_crt_bundle.h"
#endif

namespace esphome::mqtt {

ESPHOME_LOG_TAG(TAG, "mqtt");

bool MQTTBackendESP32::initialize_() {
  mqtt_cfg_.broker.address.hostname = this->host_.c_str();
  mqtt_cfg_.broker.address.port = this->port_;
  mqtt_cfg_.session.keepalive = this->keep_alive_;
  mqtt_cfg_.session.disable_clean_session = !this->clean_session_;
  // ESPHome's state machine drives reconnects, so the client stays down after disconnect()
  mqtt_cfg_.network.disable_auto_reconnect = true;

  if (!this->username_.empty()) {
    mqtt_cfg_.credentials.username = this->username_.c_str();
    if (!this->password_.empty()) {
      mqtt_cfg_.credentials.authentication.password = this->password_.c_str();
    }
  }

  if (!this->lwt_topic_.empty()) {
    mqtt_cfg_.session.last_will.topic = this->lwt_topic_.c_str();
    this->mqtt_cfg_.session.last_will.qos = this->lwt_qos_;
    this->mqtt_cfg_.session.last_will.retain = this->lwt_retain_;

    if (!this->lwt_message_.empty()) {
      mqtt_cfg_.session.last_will.msg = this->lwt_message_.c_str();
      mqtt_cfg_.session.last_will.msg_len = this->lwt_message_.size();
    }
  }

  if (!this->client_id_.empty()) {
    mqtt_cfg_.credentials.client_id = this->client_id_.c_str();
  }
#ifdef USE_MQTT_TLS
  mqtt_cfg_.broker.verification.skip_cert_common_name_check = this->skip_cert_cn_check_;
  mqtt_cfg_.broker.verification.certificate = this->ca_certificate_;
#ifdef USE_MQTT_CERT_BUNDLE
  mqtt_cfg_.broker.verification.crt_bundle_attach = esp_crt_bundle_attach;
#endif
  // Validation only accepts the client certificate together with its key
  mqtt_cfg_.credentials.authentication.certificate = this->cl_certificate_;
  mqtt_cfg_.credentials.authentication.key = this->cl_key_;
#endif
  // Validation keeps TLS off plain ws, so only the TCP transport has two forms
  switch (this->transport_) {
    case MQTTTransport::MQTT_TRANSPORT_WS:
      mqtt_cfg_.broker.address.transport = MQTT_TRANSPORT_OVER_WS;
      break;
    case MQTTTransport::MQTT_TRANSPORT_WSS:
      mqtt_cfg_.broker.address.transport = MQTT_TRANSPORT_OVER_WSS;
      break;
    case MQTTTransport::MQTT_TRANSPORT_TCP:
    default:
#ifdef USE_MQTT_TLS
      mqtt_cfg_.broker.address.transport = MQTT_TRANSPORT_OVER_SSL;
#else
      mqtt_cfg_.broker.address.transport = MQTT_TRANSPORT_OVER_TCP;
#endif
      break;
  }
  // Only set for the WebSocket transports
  mqtt_cfg_.broker.address.path = this->ws_path_;

  auto *mqtt_client = esp_mqtt_client_init(&mqtt_cfg_);
  if (mqtt_client) {
    handler_.reset(mqtt_client);
    is_initalized_ = true;
    esp_mqtt_client_register_event(mqtt_client, MQTT_EVENT_ANY, mqtt_event_handler, this);
#if defined(USE_MQTT_IDF_ENQUEUE)
    // Create the task only after MQTT client is initialized successfully
    // Use larger stack size when TLS is enabled
#ifdef USE_MQTT_TLS
    constexpr size_t stack_size = TASK_STACK_SIZE_TLS;
#else
    constexpr size_t stack_size = TASK_STACK_SIZE;
#endif
    xTaskCreate(esphome_mqtt_task, "esphome_mqtt", stack_size, (void *) this, TASK_PRIORITY, &this->task_handle_);
    if (this->task_handle_ == nullptr) {
      ESP_LOGE(TAG, "Failed to create MQTT task");
      // Clean up MQTT client since we can't start the async task
      handler_.reset();
      is_initalized_ = false;
      return false;
    }
    // Set the task handle so the queue can notify it
    this->mqtt_queue_.set_task_to_notify(this->task_handle_);
#endif
    return true;
  } else {
    ESP_LOGE(TAG, "Failed to init client");
    return false;
  }
}

void MQTTBackendESP32::loop() {
  // Capped by count and time so slow on_message automations cannot hold the loop
  const uint32_t start = App.get_loop_component_start_time();
  for (uint8_t i = 0; i < MQTT_EVENT_POOL_SIZE; i++) {
    Event *event = this->mqtt_event_queue_.pop();
    if (event == nullptr)
      break;
    const auto event_id = event->event_id;
    {
      DispatchGuard dispatching(this->dispatching_);
      this->mqtt_event_handler_(*event);
    }
    this->mqtt_event_pool_.release(event);
    // Let the client handle a connection change first
    if (event_id == MQTT_EVENT_CONNECTED || event_id == MQTT_EVENT_DISCONNECTED ||
        millis() - start >= EVENT_DRAIN_BUDGET_MS)
      break;
  }

  // A dropped DISCONNECTED, handled once the queue drains. Read task_link_state_ before empty():
  // it is cleared after the push.
  if (this->is_connected_ && this->task_link_state_.load() == LinkState::LINK_STATE_DISCONNECTED &&
      this->mqtt_event_queue_.empty()) {
    ESP_LOGW(TAG, "Disconnect event was dropped, handling it now");
    this->handle_disconnected_();
  }

  // Log dropped inbound events (check is cheap - single atomic load in common case)
  uint16_t inbound_dropped = this->mqtt_event_queue_.get_and_reset_dropped_count();
  if (inbound_dropped > 0) {
    ESP_LOGW(TAG, "Dropped %u inbound MQTT events", inbound_dropped);
  }

#if defined(USE_MQTT_IDF_ENQUEUE)
  // Periodically log dropped messages to avoid blocking during spikes.
  // During high load, many messages can be dropped in quick succession.
  // Logging each drop immediately would flood the logs and potentially
  // cause more drops if MQTT logging is enabled (cascade effect).
  // Instead, we accumulate the count and log a summary periodically.
  // IMPORTANT: Don't move this to the scheduler - if drops are due to memory
  // pressure, the scheduler's heap allocations would make things worse.
  uint32_t now = App.get_loop_component_start_time();
  // Handle rollover: (now - last_time) works correctly with unsigned arithmetic
  // even when now < last_time due to rollover
  if ((now - this->last_dropped_log_time_) >= DROP_LOG_INTERVAL_MS) {
    uint16_t dropped = this->mqtt_queue_.get_and_reset_dropped_count();
    if (dropped > 0) {
      ESP_LOGW(TAG, "Dropped %u messages (%" PRIu32 "s)", dropped, DROP_LOG_INTERVAL_MS / 1000);
    }
    this->last_dropped_log_time_ = now;
  }
#endif
}

void MQTTBackendESP32::mqtt_event_handler_(const Event &event) {
  ESP_LOGV(TAG, "Event dispatched from event loop event_id=%d", event.event_id);
  switch (event.event_id) {
    case MQTT_EVENT_BEFORE_CONNECT:
      ESP_LOGV(TAG, "MQTT_EVENT_BEFORE_CONNECT");
      break;

    case MQTT_EVENT_CONNECTED:
      ESP_LOGV(TAG, "MQTT_EVENT_CONNECTED");
      if (this->requested_link_state_.load() == LinkState::LINK_STATE_DISCONNECTED) {
        // disconnect() ran while connecting; the MQTT task closes this link
        ESP_LOGV(TAG, "Ignoring CONNECTED, disconnect pending");
        break;
      }
      this->is_connected_ = true;
      // The MQTT task already reset subacks_
      this->subscribes_sent_ = 0;
#if defined(USE_MQTT_IDF_ENQUEUE)
      this->subscribes_lost_at_connect_ = this->subscribes_lost_.load(std::memory_order_relaxed);
      this->last_dropped_log_time_ = 0;
      xTaskNotifyGive(this->task_handle_);
#endif
      this->on_connect_.call(event.session_present);
      break;
    case MQTT_EVENT_DISCONNECTED:
      ESP_LOGV(TAG, "MQTT_EVENT_DISCONNECTED");
      // TODO is there a way to get the disconnect reason?
      this->handle_disconnected_();
      break;

    case MQTT_EVENT_SUBSCRIBED:
      ESP_LOGV(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event.msg_id);
      // One topic per SUBSCRIBE, so data[0] is its return code
      this->on_subscribe_.call((int) event.msg_id,
                               event.data.empty() ? SUBACK_FAILURE : static_cast<uint8_t>(event.data[0]));
      break;
    case MQTT_EVENT_UNSUBSCRIBED:
      ESP_LOGV(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event.msg_id);
      this->on_unsubscribe_.call((int) event.msg_id);
      break;
    case MQTT_EVENT_PUBLISHED:
      ESP_LOGV(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event.msg_id);
      this->on_publish_.call((int) event.msg_id);
      break;
    case MQTT_EVENT_DATA: {
      if (!event.topic.empty()) {
        // When a single message arrives as multiple chunks, the topic will be empty
        // on any but the first message, leading to event.topic being an empty string.
        // To ensure handlers get the correct topic, cache the last seen topic to
        // simulate always receiving the topic from underlying library
        this->cached_topic_ = event.topic;
      }
      ESP_LOGV(TAG, "MQTT_EVENT_DATA %s", this->cached_topic_.c_str());
      this->on_message_.call(this->cached_topic_.c_str(), event.data.data(), event.data.size(),
                             event.current_data_offset, event.total_data_len);
    } break;
    case MQTT_EVENT_ERROR:
      ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
      if (event.error_handle.error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
        ESP_LOGE(TAG, "Last esp-tls error: 0x%x, tls stack error: 0x%x, socket errno: %d (%s)",
                 event.error_handle.esp_tls_last_esp_err, event.error_handle.esp_tls_stack_err,
                 event.error_handle.esp_transport_sock_errno, strerror(event.error_handle.esp_transport_sock_errno));
      } else if (event.error_handle.error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
        ESP_LOGE(TAG, "Connection refused error: 0x%x", event.error_handle.connect_return_code);
      } else {
        ESP_LOGE(TAG, "Unknown error type: 0x%x", event.error_handle.error_type);
      }
      break;
    default:
      ESP_LOGV(TAG, "Other event id:%d", event.event_id);
      break;
  }
}

void MQTTBackendESP32::handle_disconnected_() {
  this->is_connected_ = false;
#if defined(USE_MQTT_IDF_ENQUEUE)
  this->last_dropped_log_time_ = 0;
  xTaskNotifyGive(this->task_handle_);
#endif
  this->on_disconnect_.call(MQTTClientDisconnectReason::TCP_DISCONNECTED);
}

/// static - Dispatch event to instance method
void MQTTBackendESP32::mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id,
                                          void *event_data) {
  MQTTBackendESP32 *instance = static_cast<MQTTBackendESP32 *>(handler_args);
  if (instance == nullptr)
    return;
  const auto *mqtt_event = static_cast<esp_mqtt_event_t *>(event_data);
  // Acks are queued only when a callback wants them
  bool listened = true;
  switch (event_id) {
    case MQTT_EVENT_BEFORE_CONNECT:
      // The SDK clears a pending disconnect before this event, so request it again
      if (instance->requested_link_state_.load() == LinkState::LINK_STATE_DISCONNECTED)
        esp_mqtt_client_disconnect(mqtt_event->client);
      break;
    case MQTT_EVENT_CONNECTED:
      instance->subacks_.reset();
      instance->task_link_state_.store(LinkState::LINK_STATE_CONNECTED);  // before queueing, see task_link_state_
      break;
    case MQTT_EVENT_SUBSCRIBED:
      listened = instance->on_subscribe_.size() != 0;
      break;
    case MQTT_EVENT_UNSUBSCRIBED:
      listened = instance->on_unsubscribe_.size() != 0;
      break;
    case MQTT_EVENT_PUBLISHED: {
      // Also seen by teardown without dispatching
      auto msg_id = static_cast<uint16_t>(mqtt_event->msg_id);
      instance->acked_msg_id_.store(msg_id);
      if (msg_id == instance->tracked_msg_id_.load())
        instance->tracked_acked_.store(1);
      listened = instance->on_publish_.size() != 0;
      break;
    }
    case MQTT_EVENT_DATA:
      // Queue only the first chunk of an oversized message, so the client logs the drop
      if (mqtt_event->total_data_len > MQTT_MAX_PAYLOAD_SIZE && mqtt_event->current_data_offset != 0)
        return;
      break;
    default:
      break;
  }
  if (listened) {
    // queue event to decouple processing from ESP-IDF MQTT task to main loop
    auto *event = instance->mqtt_event_pool_.allocate();
    if (event != nullptr) {
      event->populate(*mqtt_event);
      // Push always succeeds: pool is sized to queue capacity (SIZE-1), so if
      // allocate() returned non-null, the queue cannot be full.
      instance->mqtt_event_queue_.push(event);
    } else {
      // Pool exhausted, drop event (counted via queue's dropped counter)
      instance->mqtt_event_queue_.increment_dropped_count();
    }
  } else {
    ESP_LOGV(TAG, "Ack event_id=%" PRId32 ", msg_id=%d", event_id, mqtt_event->msg_id);
  }
  if (event_id == MQTT_EVENT_DISCONNECTED) {
    // After the push, so loop() handles a queued or dropped DISCONNECTED once
    instance->task_link_state_.store(LinkState::LINK_STATE_DISCONNECTED);
  }
  if (event_id == MQTT_EVENT_SUBSCRIBED) {
    // Counted after queueing so its slot stays reserved
    // One topic per SUBSCRIBE, so data[0] is its return code
    instance->subacks_.add(mqtt_event->data_len > 0 ? static_cast<uint8_t>(mqtt_event->data[0]) : SUBACK_FAILURE);
  } else if (!listened) {
    return;
  }
  // A filtered SUBACK may still let the next SUBSCRIBE go out
  App.wake_loop_threadsafe();
}

bool MQTTBackendESP32::publish_tracked(const MQTTMessage &message) {
  this->tracked_acked_.store(0, std::memory_order_relaxed);
  // Sent directly even with idf_send_async, so the message id is known here
  int msg_id = esp_mqtt_client_publish(this->handler_.get(), message.topic.c_str(), message.payload.c_str(),
                                       message.payload.length(), std::max<uint8_t>(message.qos, 1), message.retain);
  if (msg_id <= 0)
    return false;
  this->tracked_msg_id_.store(static_cast<uint16_t>(msg_id));
  // The confirmation may have arrived before the store above
  if (this->acked_msg_id_.load() == msg_id)
    this->tracked_acked_.store(1);
  return true;
}

#if defined(USE_MQTT_IDF_ENQUEUE)
void MQTTBackendESP32::esphome_mqtt_task(void *params) {
  MQTTBackendESP32 *this_mqtt = (MQTTBackendESP32 *) params;

  while (true) {
    // Wait for notification indefinitely
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // Process all queued items
    struct QueueElement *elem;
    while ((elem = this_mqtt->mqtt_queue_.pop()) != nullptr) {
      if (this_mqtt->connected()) {
        switch (elem->type) {
          case MQTT_QUEUE_TYPE_SUBSCRIBE:
            if (esp_mqtt_client_subscribe(this_mqtt->handler_.get(), elem->topic, elem->qos) == -1)
              this_mqtt->count_lost_subscribe_();
            break;

          case MQTT_QUEUE_TYPE_UNSUBSCRIBE:
            esp_mqtt_client_unsubscribe(this_mqtt->handler_.get(), elem->topic);
            break;

          case MQTT_QUEUE_TYPE_PUBLISH:
            esp_mqtt_client_publish(this_mqtt->handler_.get(), elem->topic, elem->payload, elem->payload_len, elem->qos,
                                    elem->retain);
            break;

          default:
            ESP_LOGE(TAG, "Invalid operation type from MQTT queue");
            break;
        }
      } else if (elem->type == MQTT_QUEUE_TYPE_SUBSCRIBE) {
        this_mqtt->count_lost_subscribe_();
      }
      this_mqtt->mqtt_outbound_pool_.release(elem);
    }
  }
}

bool MQTTBackendESP32::enqueue_(MqttQueueTypeT type, const char *topic, int qos, bool retain, const char *payload,
                                size_t len) {
  auto *elem = this->mqtt_outbound_pool_.allocate();

  if (!elem) {
    // Queue is full - increment counter but don't log immediately.
    // Logging here can cause a cascade effect: if MQTT logging is enabled,
    // each dropped message would generate a log message, which could itself
    // be sent via MQTT, causing more drops and more logs in a feedback loop
    // that eventually triggers a watchdog reset. Instead, we log periodically
    // in loop() to prevent blocking the event loop during spikes.
    this->mqtt_queue_.increment_dropped_count();
    return false;
  }

  elem->type = type;
  elem->qos = qos;
  elem->retain = retain;

  // Use the helper to allocate and copy data
  if (!elem->set_data(topic, payload, len)) {
    // Allocation failed, return elem to pool
    this->mqtt_outbound_pool_.release(elem);
    // Increment counter without logging to avoid cascade effect during memory pressure
    this->mqtt_queue_.increment_dropped_count();
    return false;
  }

  // Push to queue - always succeeds since we allocated from the pool
  this->mqtt_queue_.push(elem);
  return true;
}
#endif  // USE_MQTT_IDF_ENQUEUE

}  // namespace esphome::mqtt
#endif  // USE_ESP32
#endif
