#include <string>

#include "mqtt_backend_esp8266.h"

#include "esphome/core/helpers.h"

namespace esphome {
namespace mqtt {

static const char *const TAG = "mqtt-backend-esp8266";

MQTTBackendESP8266 *object;

void MQTTBackendESP8266::on_mqtt_message_wrapper_(const char *topic, unsigned char *payload, unsigned int length) {
  object->on_mqtt_message_(topic, reinterpret_cast<const char *>(payload), length);
}

void MQTTBackendESP8266::on_mqtt_message_(const char *topic, const char *payload, unsigned int length) {
  /* no fragmented messages supported, so current_data_offset = 0 and total_data_len = length*/
  this->on_message_.call(topic, payload, length, 0, length);
}

void MQTTBackendESP8266::initialize_() {
#ifdef USE_MQTT_SECURE_CLIENT
  if (this->ca_certificate_str_.has_value()) {
    this->ca_certificate_.append(this->ca_certificate_str_.value().c_str());
    this->wifi_client_.setTrustAnchors(&this->ca_certificate_);
    if (this->skip_cert_cn_check_) {
      this->wifi_client_.setInsecure();
    }
  }
#endif

  object = this;
  mqtt_client_.setCallback(MQTTBackendESP8266::on_mqtt_message_wrapper_);
  this->is_initalized_ = true;
}

void MQTTBackendESP8266::loop() {
  if (!this->is_initalized_)
    return;
  if (this->mqtt_client_.loop()) {
    if (!this->is_connected_) {
      this->is_connected_ = true;
      /*
       * PubSubClient doesn't expose session_present flag in CONNACK, passing the clean_session flag
       * assumes the broker remembered it correctly
       */
      this->on_connect_.call(this->clean_session_);
    }
  } else {
    if (this->is_connected_) {
      this->is_connected_ = false;
      MQTTClientDisconnectReason reason = MQTTClientDisconnectReason::TCP_DISCONNECTED;
      switch (this->mqtt_client_.state()) {
        case MQTT_CONNECTION_TIMEOUT:
        case MQTT_CONNECTION_LOST:
        case MQTT_CONNECT_FAILED:
        case MQTT_DISCONNECTED:
          reason = MQTTClientDisconnectReason::TCP_DISCONNECTED;
          break;
        case MQTT_CONNECT_BAD_PROTOCOL:
          reason = MQTTClientDisconnectReason::MQTT_UNACCEPTABLE_PROTOCOL_VERSION;
          break;
        case MQTT_CONNECT_BAD_CLIENT_ID:
          reason = MQTTClientDisconnectReason::MQTT_IDENTIFIER_REJECTED;
          break;
        case MQTT_CONNECT_UNAVAILABLE:
          reason = MQTTClientDisconnectReason::MQTT_SERVER_UNAVAILABLE;
          break;
        case MQTT_CONNECT_BAD_CREDENTIALS:
          reason = MQTTClientDisconnectReason::MQTT_MALFORMED_CREDENTIALS;
          break;
        case MQTT_CONNECT_UNAUTHORIZED:
          reason = MQTTClientDisconnectReason::MQTT_NOT_AUTHORIZED;
          break;
        case MQTT_CONNECTED:
          assert(false);
          break;
      }
      this->on_disconnect_.call(reason);
    }
    char buffer[128];
    int code = this->wifi_client_.getLastSSLError(buffer, sizeof(buffer));
    if (code != 0) {
      ESP_LOGD(TAG, "SSL error code %d: %s", code, buffer);
      this->disconnect();
    }
  }
}

}  // namespace mqtt
}  // namespace esphome
