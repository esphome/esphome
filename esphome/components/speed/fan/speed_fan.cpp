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
  if (call.get_state().has_value() || call.get_speed().has_value()) {
    if (call.get_state().has_value())
      this->state = *call.get_state();
    if (call.get_speed().has_value())
      this->speed = *call.get_speed();

    float speed = this->state ? static_cast<float>(this->speed) / static_cast<float>(this->speed_count_) : 0.0f;
    this->output_->set_level(speed);
  }

  if (this->oscillating_ != nullptr && call.get_oscillating().has_value()) {
    this->oscillating = *call.get_oscillating();
    if (*call.get_oscillating()) {
      this->oscillating_->turn_on();
    } else {
      this->oscillating_->turn_off();
    }
  }

  if (this->direction_ != nullptr && call.get_direction().has_value()) {
    this->direction = *call.get_direction();
    if (*call.get_direction() == fan::FAN_DIRECTION_REVERSE) {
      this->direction_->turn_on();
    } else {
      this->direction_->turn_off();
    }
  }

  this->publish_state();
}

}  // namespace speed
}  // namespace esphome
