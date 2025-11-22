#include "mqtt_text_sensor.h"
#include "esphome/core/log.h"

#include "mqtt_const.h"

#ifdef USE_MQTT
#ifdef USE_TEXT_SENSOR

namespace esphome {
namespace mqtt {

static const char *const TAG = "mqtt.text_sensor";

using namespace esphome::text_sensor;

MQTTTextSensor::MQTTTextSensor(TextSensor *sensor) : sensor_(sensor) {}
void MQTTTextSensor::send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) {
  // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  const auto device_class = this->sensor_->get_device_class_ref();
  if (!device_class.empty()) {
    root[MQTT_DEVICE_CLASS] = device_class;
  }
  // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)
  config.command_topic = false;
}
void MQTTTextSensor::setup() {
  this->sensor_->add_on_state_callback([this](const std::string &state) { this->publish_state(state); });
}

void MQTTTextSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "MQTT Text Sensor '%s':", this->sensor_->get_name().c_str());
  LOG_MQTT_COMPONENT(true, false);
}

bool MQTTTextSensor::publish_state(const std::string &value) { return this->publish(this->get_state_topic_(), value); }
bool MQTTTextSensor::send_initial_state() {
  if (this->sensor_->has_state()) {
    return this->publish_state(this->sensor_->state);
  } else {
    return true;
  }
}
std::string MQTTTextSensor::component_type() const { return "sensor"; }
const EntityBase *MQTTTextSensor::get_entity() const { return this->sensor_; }

}  // namespace mqtt
}  // namespace esphome

#endif
#endif  // USE_MQTT
