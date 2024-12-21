#include "mqtt_backend_esp32.h"

#ifdef USE_MQTT
#ifdef USE_ESP32

#include <string>
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace mqtt {

static const char *const TAG = "mqtt.idf";

bool MQTTBackendESP32::initialize_() {
#if ESP_IDF_VERSION_MAJOR < 5
  mqtt_cfg_.user_context = (void *) this;
  mqtt_cfg_.buffer_size = MQTT_BUFFER_SIZE;

  mqtt_cfg_.host = this->host_.c_str();
  mqtt_cfg_.port = this->port_;
  mqtt_cfg_.keepalive = this->keep_alive_;
  mqtt_cfg_.disable_clean_session = !this->clean_session_;

  if (!this->username_.empty()) {
    mqtt_cfg_.username = this->username_.c_str();
    if (!this->password_.empty()) {
      mqtt_cfg_.password = this->password_.c_str();
    }
  }

  if (!this->lwt_topic_.empty()) {
    mqtt_cfg_.lwt_topic = this->lwt_topic_.c_str();
    this->mqtt_cfg_.lwt_qos = this->lwt_qos_;
    this->mqtt_cfg_.lwt_retain = this->lwt_retain_;

    if (!this->lwt_message_.empty()) {
      mqtt_cfg_.lwt_msg = this->lwt_message_.c_str();
      mqtt_cfg_.lwt_msg_len = this->lwt_message_.size();
    }
  }

  if (!this->client_id_.empty()) {
    mqtt_cfg_.client_id = this->client_id_.c_str();
  }
  if (ca_certificate_.has_value()) {
    mqtt_cfg_.cert_pem = ca_certificate_.value().c_str();
    mqtt_cfg_.skip_cert_common_name_check = skip_cert_cn_check_;
    mqtt_cfg_.transport = MQTT_TRANSPORT_OVER_SSL;

    if (this->cl_certificate_.has_value() && this->cl_key_.has_value()) {
      mqtt_cfg_.client_cert_pem = this->cl_certificate_.value().c_str();
      mqtt_cfg_.client_key_pem = this->cl_key_.value().c_str();
    }
  } else {
    mqtt_cfg_.transport = MQTT_TRANSPORT_OVER_TCP;
  }
#else
  mqtt_cfg_.broker.address.hostname = this->host_.c_str();
  mqtt_cfg_.broker.address.port = this->port_;
  mqtt_cfg_.session.keepalive = this->keep_alive_;
  mqtt_cfg_.session.disable_clean_session = !this->clean_session_;

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
  if (ca_certificate_.has_value()) {
    mqtt_cfg_.broker.verification.certificate = ca_certificate_.value().c_str();
    mqtt_cfg_.broker.verification.skip_cert_common_name_check = skip_cert_cn_check_;
    mqtt_cfg_.broker.address.transport = MQTT_TRANSPORT_OVER_SSL;

    if (this->cl_certificate_.has_value() && this->cl_key_.has_value()) {
      mqtt_cfg_.credentials.authentication.certificate = this->cl_certificate_.value().c_str();
      mqtt_cfg_.credentials.authentication.key = this->cl_key_.value().c_str();
    }
  } else {
    mqtt_cfg_.broker.address.transport = MQTT_TRANSPORT_OVER_TCP;
  }
#endif
  auto *mqtt_client = esp_mqtt_client_init(&mqtt_cfg_);
  if (mqtt_client) {
    handler_.reset(mqtt_client);
    is_initalized_ = true;
    esp_mqtt_client_register_event(mqtt_client, MQTT_EVENT_ANY, mqtt_event_handler, this);
    return true;
  } else {
    ESP_LOGE(TAG, "Failed to initialize IDF-MQTT");
    return false;
  }
}

void MQTTBackendESP32::loop() {
  // process new events
  // handle only 1 message per loop iteration
  if (!mqtt_events_.empty()) {
    auto &event = mqtt_events_.front();
    mqtt_event_handler_(event);
    mqtt_events_.pop();
  }
}

void MQTTBackendESP32::mqtt_event_handler_(const Event &event) {
  ESP_LOGV(TAG, "Event dispatched from event loop event_id=%d", event.event_id);
  switch (event.event_id) {
    case MQTT_EVENT_BEFORE_CONNECT:
      ESP_LOGV(TAG, "MQTT_EVENT_BEFORE_CONNECT");
      break;

    case MQTT_EVENT_CONNECTED:
      ESP_LOGV(TAG, "MQTT_EVENT_CONNECTED");
      this->is_connected_ = true;
      this->on_connect_.call(event.session_present);
      break;
    case MQTT_EVENT_DISCONNECTED:
      ESP_LOGV(TAG, "MQTT_EVENT_DISCONNECTED");
      // TODO is there a way to get the disconnect reason?
      this->is_connected_ = false;
      this->on_disconnect_.call(MQTTClientDisconnectReason::TCP_DISCONNECTED);
      break;

    case MQTT_EVENT_SUBSCRIBED:
      ESP_LOGV(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event.msg_id);
      // hardcode QoS to 0. QoS is not used in this context but required to mirror the AsyncMqtt interface
      this->on_subscribe_.call((int) event.msg_id, 0);
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
      static std::string topic;
      if (!event.topic.empty()) {
        topic = event.topic;
      }
      ESP_LOGV(TAG, "MQTT_EVENT_DATA %s", topic.c_str());
      this->on_message_.call(!event.topic.empty() ? topic.c_str() : nullptr, event.data.data(), event.data.size(),
                             event.current_data_offset, event.total_data_len);
    } break;
    case MQTT_EVENT_ERROR:
      ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
      if (event.error_handle.error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
        ESP_LOGE(TAG, "Last error code reported from esp-tls: 0x%x", event.error_handle.esp_tls_last_esp_err);
        ESP_LOGE(TAG, "Last tls stack error number: 0x%x", event.error_handle.esp_tls_stack_err);
        ESP_LOGE(TAG, "Last captured errno : %d (%s)", event.error_handle.esp_transport_sock_errno,
                 strerror(event.error_handle.esp_transport_sock_errno));
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

/// static - Dispatch event to instance method
void MQTTBackendESP32::mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id,
                                          void *event_data) {
  MQTTBackendESP32 *instance = static_cast<MQTTBackendESP32 *>(handler_args);
  // queue event to decouple processing
  if (instance) {
    auto event = *static_cast<esp_mqtt_event_t *>(event_data);
    instance->mqtt_events_.emplace(event);
  }
}

}  // namespace mqtt
}  // namespace esphome
#endif  // USE_ESP32
#endif
