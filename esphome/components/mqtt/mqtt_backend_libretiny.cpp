#include "mqtt_backend_libretiny.h"

#ifdef USE_MQTT
#ifdef USE_LIBRETINY

#include <string>

#include "esphome/core/helpers.h"

namespace esphome {
namespace mqtt {

static const char *const TAG = "mqtt-backend-libretiny";

void MQTTBackendLibreTiny::on_mqtt_message_wrapper_(MQTTClient *client, char topic[], char bytes[], int length) {
  static_cast<MQTTBackendLibreTiny *>(client->ref)->on_mqtt_message_(client, topic, bytes, length);
}

void MQTTBackendLibreTiny::on_mqtt_message_(MQTTClient *client, char topic[], char bytes[], int length) {
  this->on_message_.call(topic, bytes, length, 0, length);
}

void MQTTBackendLibreTiny::initialize_() {
  this->mqtt_client_.ref = this;
  mqtt_client_.onMessageAdvanced(MQTTBackendLibreTiny::on_mqtt_message_wrapper_);
  this->is_initalized_ = true;
}

void MQTTBackendLibreTiny::handleErrors_() {
  lwmqtt_err_t error = this->mqtt_client_.lastError();
  lwmqtt_return_code_t return_code = this->mqtt_client_.returnCode();
  if (error != LWMQTT_SUCCESS) {
    ESP_LOGD(TAG, "Error: %d, returnCode: %d", error, return_code);

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

void MQTTBackendLibreTiny::connect() {
  if (!this->is_initalized_) {
    this->initialize_();
  }
  this->mqtt_client_.begin(this->host_.c_str(), this->port_, this->wifi_client_);
  this->mqtt_client_.connect(this->client_id_.c_str(), this->username_.c_str(), this->password_.c_str());
  this->handleErrors_();
}

void MQTTBackendLibreTiny::loop() {
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

#endif
#endif
