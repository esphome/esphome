#include "outdoor_fallback_binary_sensor.h"
#include "esphome/core/log.h"
#include "../equitherm.h"

namespace esphome {
namespace equitherm {

static const char *const TAG = "outdoor_fallback_binary_sensor";

void OutdoorFallbackBinarySensor::setup() {
  this->parent_->add_on_state_callback([this]() { this->update_from_parent_(); });
  this->update_from_parent_();
}

void OutdoorFallbackBinarySensor::update_from_parent_() {
  bool state = this->parent_->is_outdoor_fallback_active();
  this->publish_state(state);
}

void OutdoorFallbackBinarySensor::dump_config() { LOG_BINARY_SENSOR("", "OutdoorFallbackBinarySensor", this); }

}  // namespace equitherm
}  // namespace esphome
