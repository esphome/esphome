#include "binary_fan.h"
#include "esphome/core/log.h"

namespace esphome {
namespace binary {

static const char *const TAG = "binary.fan";

void BinaryFan::dump_config() { LOG_FAN("", "Binary Fan", this); }
fan::FanTraits BinaryFan::get_traits() {
  return fan::FanTraits(this->oscillating_ != nullptr, false, this->direction_ != nullptr, 0);
}
void BinaryFan::control(const fan::FanCall &call) {
  if (call.get_state().has_value()) {
    this->state = *call.get_state();
    if (*call.get_state()) {
      this->output_->turn_on();
    } else {
      this->output_->turn_off();
    }
  }

  if (call.get_oscillating().has_value()) {
    this->oscillating = *call.get_oscillating();
    if (*call.get_oscillating()) {
      this->oscillating_->turn_on();
    } else {
      this->oscillating_->turn_off();
    }
  }

  if (call.get_direction().has_value()) {
    this->direction = *call.get_direction();
    if (*call.get_direction() == fan::FAN_DIRECTION_REVERSE) {
      this->direction_->turn_on();
    } else {
      this->direction_->turn_off();
    }
  }

  this->publish_state();
}

}  // namespace binary
}  // namespace esphome
