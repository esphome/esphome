#include <string>

#include "mqtt_backend_esp8266.h"

#include "esphome/core/helpers.h"

namespace esphome {
namespace mqtt {

static const char *const TAG = "mqtt-backend-esp8266";

void MQTTBackendESP8266::on_mqtt_message_wrapper_(MQTTClient *client, char topic[], char bytes[], int length) {
  static_cast<MQTTBackendESP8266 *>(client->ref)->on_mqtt_message_(client, topic, bytes, length);
}

void MQTTBackendESP8266::on_mqtt_message_(MQTTClient *client, char topic[], char bytes[], int length) {
  this->on_message_.call(topic, bytes, length, 0, length);
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

  this->mqtt_client_.ref = this;
  mqtt_client_.onMessageAdvanced(MQTTBackendESP8266::on_mqtt_message_wrapper_);
  this->is_initalized_ = true;
}

void MQTTBackendESP8266::handleErrors_() {
  lwmqtt_err_t error = this->mqtt_client_.lastError();
  lwmqtt_return_code_t return_code = this->mqtt_client_.returnCode();
  if (error != LWMQTT_SUCCESS) {
    ESP_LOGD(TAG, "Error: %d, returnCode: %d", error, return_code);

    char buffer[128];
    int code = this->wifi_client_.getLastSSLError(buffer, sizeof(buffer));
    if (code != 0) {
      ESP_LOGD(TAG, "SSL error code %d: %s", code, buffer);
    }

    MQTTClientDisconnectReason reason = MQTTClientDisconnectReason::TCP_DISCONNECTED;

    if (return_code != LWMQTT_CONNECTION_ACCEPTED) {
      switch (return_code) {
        case LWMQTT_CONNECTION_ACCEPTED:
          assert(false);
          break;
        case LWMQTT_UNACCEPTABLE_PROTOCOL:
          reason = MQTTClientDisconnectReason::MQTT_UNACCEPTABLE_PROTOCOL_VERSION;
          break;
        case LWMQTT_IDENTIFIER_REJECTED:
          reason = MQTTClientDisconnectReason::MQTT_IDENTIFIER_REJECTED;
          break;
        case LWMQTT_SERVER_UNAVAILABLE:
          reason = MQTTClientDisconnectReason::MQTT_SERVER_UNAVAILABLE;
          break;
        case LWMQTT_BAD_USERNAME_OR_PASSWORD:
          reason = MQTTClientDisconnectReason::MQTT_MALFORMED_CREDENTIALS;
          break;
        case LWMQTT_NOT_AUTHORIZED:
          reason = MQTTClientDisconnectReason::MQTT_NOT_AUTHORIZED;
          break;
        case LWMQTT_UNKNOWN_RETURN_CODE:
          reason = MQTTClientDisconnectReason::TCP_DISCONNECTED;
          break;
      }
    }
    this->on_disconnect_.call(reason);
  }
}

void MQTTBackendESP8266::connect() {
  if (!this->is_initalized_) {
    this->initialize_();
  }
  this->mqtt_client_.begin(this->host_.c_str(), this->port_, this->wifi_client_);
  this->mqtt_client_.connect(this->client_id_.c_str(), this->username_.c_str(), this->password_.c_str());
  this->handleErrors_();
}

void MQTTBackendESP8266::loop() {
  this->mqtt_client_.loop();
  if (!this->is_connected_ && this->mqtt_client_.connected()) {
    this->is_connected_ = true;
    this->on_connect_.call(this->clean_session_);
  }
  if (this->is_connected_ && !this->mqtt_client_.connected()) {
    this->is_connected_ = false;
    this->on_disconnect_.call(MQTTClientDisconnectReason::TCP_DISCONNECTED);
  }
}

}  // namespace mqtt
}  // namespace esphome
