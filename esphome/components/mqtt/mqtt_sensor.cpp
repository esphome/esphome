#include "mqtt_sensor.h"
#include "esphome/core/log.h"

#ifdef USE_SENSOR

#ifdef USE_DEEP_SLEEP
#include "esphome/components/deep_sleep/deep_sleep_component.h"
#endif

namespace esphome {
namespace mqtt {

static const char *TAG = "mqtt.sensor";

using namespace esphome::sensor;

MQTTSensorComponent::MQTTSensorComponent(Sensor *sensor) : MQTTComponent(), sensor_(sensor) {}

void MQTTSensorComponent::setup() {
  this->sensor_->add_on_state_callback([this](float state) { this->publish_state(state); });
}

void MQTTSensorComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MQTT Sensor '%s':", this->sensor_->get_name().c_str());
  if (this->get_expire_after() > 0) {
    ESP_LOGCONFIG(TAG, "  Expire After: %us", this->get_expire_after() / 1000);
  }
  LOG_MQTT_COMPONENT(true, false)
}

std::string MQTTSensorComponent::component_type() const { return "sensor"; }

uint32_t MQTTSensorComponent::get_expire_after() const {
  if (this->expire_after_.has_value()) {
    return *this->expire_after_;
  } else {
#ifdef USE_DEEP_SLEEP
    if (deep_sleep::global_has_deep_sleep) {
      return 0;
    }
#endif
    return this->sensor_->calculate_expected_filter_update_interval() * 5;
  }
}
void MQTTSensorComponent::set_expire_after(uint32_t expire_after) { this->expire_after_ = expire_after; }
void MQTTSensorComponent::disable_expire_after() { this->expire_after_ = 0; }
std::string MQTTSensorComponent::friendly_name() const { return this->sensor_->get_name(); }
void MQTTSensorComponent::send_discovery(JsonObject &root, mqtt::SendDiscoveryConfig &config) {
  if (!this->sensor_->get_unit_of_measurement().empty())
    root["unit_of_measurement"] = this->sensor_->get_unit_of_measurement();

  if (this->get_expire_after() > 0)
    root["expire_after"] = this->get_expire_after() / 1000;

  if (!this->sensor_->get_icon().empty())
    root["icon"] = this->sensor_->get_icon();

  if (this->sensor_->get_force_update())
    root["force_update"] = true;

  config.command_topic = false;
}
bool MQTTSensorComponent::send_initial_state() {
  if (this->sensor_->has_state()) {
    return this->publish_state(this->sensor_->state);
  } else {
    return true;
  }
}
bool MQTTSensorComponent::is_internal() { return this->sensor_->is_internal(); }
bool MQTTSensorComponent::publish_state(float value) {
  int8_t accuracy = this->sensor_->get_accuracy_decimals();
  return this->publish(this->get_state_topic_(), value_accuracy_to_string(value, accuracy));
}
std::string MQTTSensorComponent::unique_id() { return this->sensor_->unique_id(); }

}  // namespace mqtt
}  // namespace esphome

#endif
