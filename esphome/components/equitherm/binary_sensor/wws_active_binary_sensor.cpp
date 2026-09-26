#include "wws_active_binary_sensor.h"
#include "esphome/core/log.h"
#include "../equitherm.h"

namespace esphome::equitherm {

static const char *const TAG = "wws_active_binary_sensor";

void WwsActiveBinarySensor::setup() {
  this->parent_->add_on_state_callback([this]() { this->update_from_parent_(); });
  this->update_from_parent_();
}

void WwsActiveBinarySensor::update_from_parent_() {
  bool state = this->parent_->is_wws_active();
  this->publish_state(state);
}

void WwsActiveBinarySensor::dump_config() { LOG_BINARY_SENSOR("", "WwsActiveBinarySensor", this); }

}  // namespace esphome::equitherm
