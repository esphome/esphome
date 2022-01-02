#ifdef USE_ESP_IDF
#include <string>
#include "mqtt_client_idf.h"
#include "lwip/ip_addr.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/components/network/ip_address.h"
namespace esphome {
namespace mqtt {
static const char *const TAG = "mqtt.idf";

bool MqttIdfClient::initialize_() {
  mqtt_cfg_.user_context = (void *) this;
  mqtt_cfg_.buffer_size = MQTT_BUFFER_SIZE;

  mqtt_cfg_.host = this->host_.c_str();
  mqtt_cfg_.port = this->port_;
  mqtt_cfg_.keepalive = this->keep_alive_;
  mqtt_cfg_.disable_clean_session = !this->clean_session_;

  if (this->username_.empty()) {
    mqtt_cfg_.username = this->username_.c_str();
    if (this->password_.empty()) {
      mqtt_cfg_.password = this->password_.c_str();
    }
  }

  if (!this->lwt_topic_.empty()) {
    mqtt_cfg_.lwt_topic = this->lwt_topic_.c_str();
    // this->mqtt_cfg_.lwt_qos = this->lwt_qos_;
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
  } else {
    mqtt_cfg_.transport = MQTT_TRANSPORT_OVER_TCP;
  }

  auto mqtt_client = esp_mqtt_client_init(&mqtt_cfg_);
  if (mqtt_client) {
    handler_.reset(mqtt_client);
    is_initalized_ = true;
    esp_mqtt_client_register_event(mqtt_client, MQTT_EVENT_ANY, mqtt_event_handler, this);
    return true;
  } else {
    ESP_LOGE(TAG, "Failed to initialize IDF-MQTT");
  }
  return false;
}

void MqttIdfClient::mqtt_event_handler_(esp_event_base_t base, int32_t event_id, void *event_data) {
  auto *event = static_cast<esp_mqtt_event_t *>(event_data);
  ESP_LOGV(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
  switch (event->event_id) {
    case MQTT_EVENT_BEFORE_CONNECT:
      ESP_LOGV(TAG, "MQTT_EVENT_BEFORE_CONNECT");
      break;

    case MQTT_EVENT_CONNECTED:
      ESP_LOGV(TAG, "MQTT_EVENT_CONNECTED");
      // TODO session present check
      this->is_connected_ = true;
      for (auto &callback : this->on_connect_) {
        callback(!mqtt_cfg_.disable_clean_session);
      }
      break;
    case MQTT_EVENT_DISCONNECTED:
      ESP_LOGV(TAG, "MQTT_EVENT_DISCONNECTED");
      // TODO is there a way to get the disconnect reason?
      this->is_connected_ = false;
      for (auto &callback : this->on_disconnect_) {
        callback(MqttClientDisconnectReason::TCP_DISCONNECTED);
      }
      break;

    case MQTT_EVENT_SUBSCRIBED:
      ESP_LOGV(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
      // hardocde QoS to 0. QoS is not used in this context but required to mirror the AsyncMqtt interface
      for (auto &callback : this->on_subscribe_) {
        callback((int) event->msg_id, 0);
      }
      break;
    case MQTT_EVENT_UNSUBSCRIBED:
      ESP_LOGV(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
      for (auto &callback : this->on_unsubscribe_) {
        callback((int) event->msg_id);
      }
      break;
    case MQTT_EVENT_PUBLISHED:
      ESP_LOGV(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
      for (auto &callback : this->on_publish_) {
        callback((int) event->msg_id);
      }
      break;
    case MQTT_EVENT_DATA: {
      static std::string topic;
      if (event->topic) {
        // not 0 terminated - create a string from it
        topic = std::string(event->topic, event->topic_len);
      }
      ESP_LOGV(TAG, "MQTT_EVENT_DATA %s", topic.c_str());
      auto data_len = event->data_len;
      if (data_len == 0)
        data_len = strlen(event->data);
      for (auto &callback : this->on_message_) {
        callback(event->topic ? const_cast<char *>(topic.c_str()) : nullptr, event->data, data_len,
                 event->current_data_offset, event->total_data_len);
      }
    } break;
    case MQTT_EVENT_ERROR:
      ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
      if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
        ESP_LOGE(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
        ESP_LOGE(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
        ESP_LOGE(TAG, "Last captured errno : %d (%s)", event->error_handle->esp_transport_sock_errno,
                 strerror(event->error_handle->esp_transport_sock_errno));
      } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
        ESP_LOGE(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
      } else {
        ESP_LOGE(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
      }
      break;
    default:
      ESP_LOGV(TAG, "Other event id:%d", event->event_id);
      break;
  }
}

/// static - Dispatch event to instance method
void MqttIdfClient::mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
  MqttIdfClient *instance = static_cast<MqttIdfClient *>(handler_args);
  if (instance)
    instance->mqtt_event_handler_(base, event_id, event_data);
}

}  // namespace mqtt
}  // namespace esphome
#endif  // USE_ESP_IDF
