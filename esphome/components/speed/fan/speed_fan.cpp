#include "speed_fan.h"
#include "esphome/core/log.h"

namespace esphome {
namespace speed {

static const char *const TAG = "speed.fan";

void SpeedFan::setup() {
  // Construct traits before restore so preset modes can be looked up by index
  this->traits_ = fan::FanTraits(this->oscillating_ != nullptr, true, this->direction_ != nullptr, this->speed_count_);
  this->traits_.set_supported_preset_modes(this->preset_modes_);

  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->apply(*this);
    this->write_state_();
  }
}

void SpeedFan::dump_config() { LOG_FAN("", "Speed Fan", this); }

void SpeedFan::control(const fan::FanCall &call) {
  if (auto val = call.get_state(); val.has_value())
    this->state = *val;
  if (auto val = call.get_speed(); val.has_value())
    this->speed = *val;
  if (auto val = call.get_oscillating(); val.has_value())
    this->oscillating = *val;
  if (auto val = call.get_direction(); val.has_value())
    this->direction = *val;
  this->apply_preset_mode_(call);

  this->write_state_();
  this->publish_state();
}

void SpeedFan::write_state_() {
  float speed = this->state ? static_cast<float>(this->speed) / static_cast<float>(this->speed_count_) : 0.0f;
  this->output_->set_level(speed);
  if (this->oscillating_ != nullptr)
    this->oscillating_->set_state(this->oscillating);
  if (this->direction_ != nullptr)
    this->direction_->set_state(this->direction == fan::FanDirection::REVERSE);
}

}  // namespace speed
}  // namespace esphome
