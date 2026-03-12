#include "indoor_fallback_binary_sensor.h"
#include "esphome/core/log.h"
#include "../equitherm.h"

namespace esphome {
namespace equitherm {

static const char *const TAG = "indoor_fallback_binary_sensor";

void IndoorFallbackBinarySensor::setup() {
  this->parent_->add_on_state_callback([this]() { this->update_from_parent_(); });
  this->update_from_parent_();
}

void IndoorFallbackBinarySensor::update_from_parent_() {
  bool state = this->parent_->is_indoor_fallback_active();
  this->publish_state(state);
}

void IndoorFallbackBinarySensor::dump_config() { LOG_BINARY_SENSOR("", "IndoorFallbackBinarySensor", this); }

}  // namespace equitherm
}  // namespace esphome
