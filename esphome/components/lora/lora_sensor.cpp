#include "lora_sensor.h"
#include "esphome/core/log.h"

#ifdef USE_SENSOR

#ifdef USE_DEEP_SLEEP
#include "esphome/components/deep_sleep/deep_sleep_component.h"
#endif

namespace esphome {
namespace lora {

static const char *TAG = "lora.sensor";

using namespace esphome::sensor;

LoraSensorComponent::LoraSensorComponent(Sensor *sensor) : LoraComponent(), sensor_(sensor) {}

void LoraSensorComponent::setup() {
  this->sensor_->add_on_state_callback([this](float state) { this->publish_state(state); });
}

void LoraSensorComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MQTT Sensor '%s':", this->sensor_->get_name().c_str());
  if (this->get_expire_after() > 0) {
    ESP_LOGCONFIG(TAG, "  Expire After: %us", this->get_expire_after() / 1000);
  }
  LOG_MQTT_COMPONENT(true, false)
}

std::string LoraSensorComponent::component_type() const { return "sensor"; }

std::string LoraSensorComponent::friendly_name() const { return this->sensor_->get_name(); }

bool LoraSensorComponent::send_initial_state() {
  if (this->sensor_->has_state()) {
    return this->publish_state(this->sensor_->state);
  } else {
    return true;
  }
}
bool LoraSensorComponent::is_internal() { return this->sensor_->is_internal(); }
bool LoraSensorComponent::publish_state(float value) {
  int8_t accuracy = this->sensor_->get_accuracy_decimals();
  return this->publish(this->get_state_topic_(), value_accuracy_to_string(value, accuracy));
}
std::string LoraSensorComponent::unique_id() { return this->sensor_->unique_id(); }

}  // namespace lora
}  // namespace esphome

#endif
