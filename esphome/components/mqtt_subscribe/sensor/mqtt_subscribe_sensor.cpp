#include "mqtt_subscribe_sensor.h"

#ifdef USE_MQTT

#include "esphome/core/log.h"

namespace esphome {
namespace mqtt_subscribe {

static const char *const TAG = "mqtt_subscribe.sensor";

void MQTTSubscribeSensor::setup() {
  mqtt::global_mqtt_client->subscribe(
      this->topic_,
      [this](const std::string &topic, const std::string &payload) {
        auto val = parse_number<float>(payload);
        if (!val.has_value()) {
          ESP_LOGW(TAG, "Can't convert '%s' to number!", payload.c_str());
          this->publish_state(NAN);
          return;
        }

        this->publish_state(*val);
      },
      this->qos_);
}

float MQTTSubscribeSensor::get_setup_priority() const { return setup_priority::AFTER_CONNECTION; }
void MQTTSubscribeSensor::set_qos(uint8_t qos) { this->qos_ = qos; }
void MQTTSubscribeSensor::dump_config() {
  LOG_SENSOR("", "MQTT Subscribe", this);
  ESP_LOGCONFIG(TAG, "  Topic: %s", this->topic_.c_str());
}

}  // namespace mqtt_subscribe
}  // namespace esphome

#endif  // USE_MQTT
