#include "speed_fan.h"
#include "esphome/components/fan/fan_helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace speed {

static const char *const TAG = "speed.fan";

void SpeedFan::dump_config() { LOG_FAN("", "Speed Fan", this); }
fan::FanTraits SpeedFan::get_traits() {
  return fan::FanTraits(this->oscillating_ != nullptr, true, this->direction_ != nullptr, this->speed_count_);
}
void SpeedFan::control(const fan::FanCall &call) {
  {
    float speed = 0.0f;
    if (call.get_state().value_or(this->state)) {
      speed = static_cast<float>(call.get_speed().value_or(this->speed)) / static_cast<float>(this->speed_count_);
    }
    this->output_->set_level(speed);
  }

  if (this->oscillating_ != nullptr && call.get_oscillating().has_value()) {
    if (*call.get_oscillating()) {
      this->oscillating_->turn_on();
    } else {
      this->oscillating_->turn_off();
    }
  }

  if (this->direction_ != nullptr && call.get_direction().has_value()) {
    if (*call.get_direction() == fan::FAN_DIRECTION_REVERSE) {
      this->direction_->turn_on();
    } else {
      this->direction_->turn_off();
    }
  }
}

}  // namespace speed
}  // namespace esphome
