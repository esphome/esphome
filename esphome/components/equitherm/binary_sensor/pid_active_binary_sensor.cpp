#include "pid_active_binary_sensor.h"
#include "esphome/core/log.h"
#include "../equitherm.h"

namespace esphome {
namespace equitherm {

static const char *const TAG = "pid_active_binary_sensor";

void PidActiveBinarySensor::setup() {
  this->parent_->add_on_state_callback([this]() { this->update_from_parent_(); });
  this->update_from_parent_();
}

void PidActiveBinarySensor::update_from_parent_() {
  bool state = this->parent_->is_pid_active();
  this->publish_state(state);
}

void PidActiveBinarySensor::dump_config() { LOG_BINARY_SENSOR("", "PidActiveBinarySensor", this); }

}  // namespace equitherm
}  // namespace esphome
