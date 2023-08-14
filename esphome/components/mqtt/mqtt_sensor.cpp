#include <cinttypes>
#include "mqtt_sensor.h"
#include "esphome/core/log.h"

#include "mqtt_const.h"

#ifdef USE_MQTT
#ifdef USE_SENSOR

#ifdef USE_DEEP_SLEEP
#include "esphome/components/deep_sleep/deep_sleep_component.h"
#endif

namespace esphome {
namespace mqtt {

static const char *const TAG = "mqtt.sensor";

using namespace esphome::sensor;

MQTTSensorComponent::MQTTSensorComponent(Sensor *sensor) : sensor_(sensor) {}

void MQTTSensorComponent::setup() {
  this->sensor_->add_on_state_callback([this](float state) { this->publish_state(state); });
}

void MQTTSensorComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MQTT Sensor '%s':", this->sensor_->get_name().c_str());
  if (this->get_expire_after() > 0) {
    ESP_LOGCONFIG(TAG, "  Expire After: %" PRIu32 "s", this->get_expire_after() / 1000);
  }
  LOG_MQTT_COMPONENT(true, false)
}

std::string MQTTSensorComponent::component_type() const { return "sensor"; }
const EntityBase *MQTTSensorComponent::get_entity() const { return this->sensor_; }

uint32_t MQTTSensorComponent::get_expire_after() const {
  if (this->expire_after_.has_value())
    return *this->expire_after_;
  return 0;
}
void MQTTSensorComponent::set_expire_after(uint32_t expire_after) { this->expire_after_ = expire_after; }
void MQTTSensorComponent::disable_expire_after() { this->expire_after_ = 0; }

void MQTTSensorComponent::send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) {
  if (!this->sensor_->get_device_class().empty())
    root[MQTT_DEVICE_CLASS] = this->sensor_->get_device_class();

  if (!this->sensor_->get_unit_of_measurement().empty())
    root[MQTT_UNIT_OF_MEASUREMENT] = this->sensor_->get_unit_of_measurement();

  if (this->get_expire_after() > 0)
    root[MQTT_EXPIRE_AFTER] = this->get_expire_after() / 1000;

  if (this->sensor_->get_force_update())
    root[MQTT_FORCE_UPDATE] = true;

  if (this->sensor_->get_state_class() != STATE_CLASS_NONE)
    root[MQTT_STATE_CLASS] = state_class_to_string(this->sensor_->get_state_class());

  config.command_topic = false;
}
bool MQTTSensorComponent::send_initial_state() {
  if (this->sensor_->has_state()) {
    return this->publish_state(this->sensor_->state);
  } else {
    return true;
  }
}
bool MQTTSensorComponent::publish_state(float value) {
  int8_t accuracy = this->sensor_->get_accuracy_decimals();
  return this->publish(this->get_state_topic_(), value_accuracy_to_string(value, accuracy));
}
std::string MQTTSensorComponent::unique_id() { return this->sensor_->unique_id(); }

}  // namespace mqtt
}  // namespace esphome

#endif
#endif  // USE_MQTT
