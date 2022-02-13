#include "mqtt_subscribe_text_sensor.h"

#ifdef USE_MQTT

#include "esphome/core/log.h"
#include <utility>

namespace esphome {
namespace mqtt_subscribe {

static const char *const TAG = "mqtt_subscribe.text_sensor";

void MQTTSubscribeTextSensor::setup() {
  this->parent_->subscribe(
      this->topic_, [this](const std::string &topic, const std::string &payload) { this->publish_state(payload); },
      this->qos_);
}
float MQTTSubscribeTextSensor::get_setup_priority() const { return setup_priority::AFTER_CONNECTION; }
void MQTTSubscribeTextSensor::set_qos(uint8_t qos) { this->qos_ = qos; }
void MQTTSubscribeTextSensor::dump_config() {
  LOG_TEXT_SENSOR("", "MQTT Subscribe Text Sensor", this);
  ESP_LOGCONFIG(TAG, "  Topic: %s", this->topic_.c_str());
}

}  // namespace mqtt_subscribe
}  // namespace esphome

#endif  // USE_MQTT
