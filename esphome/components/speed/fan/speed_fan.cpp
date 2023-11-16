#include "speed_fan.h"
#include "esphome/core/log.h"

namespace esphome {
namespace speed {

static const char *const TAG = "speed.fan";

void SpeedFan::setup() {
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->apply(*this);
    this->write_state_();
  }

  // Construct traits
  this->traits_ = fan::FanTraits(this->oscillating_ != nullptr, true, this->direction_ != nullptr, this->speed_count_);

  // Add all presets to traits
  std::vector<std::string> keys;
  for (auto const &kv : this->preset_modes_)
    keys.push_back(kv.first);

  this->traits_.set_supported_preset_modes(keys);
}

void SpeedFan::dump_config() { LOG_FAN("", "Speed Fan", this); }

void SpeedFan::control(const fan::FanCall &call) {
  if (call.get_state().has_value())
    this->state = *call.get_state();
  if (call.get_speed().has_value())
    this->speed = *call.get_speed();
  if (call.get_oscillating().has_value())
    this->oscillating = *call.get_oscillating();
  if (call.get_direction().has_value())
    this->direction = *call.get_direction();

  // Recursively call the control function with the preset's stored FanCall if it's changed
  const auto last_preset = this->preset_mode;
  this->preset_mode = call.get_preset_mode();

  if (!this->preset_mode.empty() && this->preset_mode != last_preset)
    return this->control(this->preset_modes_.at(this->preset_mode));

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

void SpeedFan::add_preset_mode(const std::string &name, optional<int> speed, optional<bool> oscillating,
                               optional<fan::FanDirection> direction) {
  auto call = this->make_call();

  call.set_preset_mode(name);

  if (speed)
    call.set_speed(*speed);

  if (oscillating)
    call.set_oscillating(*oscillating);

  if (direction)
    call.set_direction(static_cast<fan::FanDirection>(*direction));

  this->preset_modes_.emplace(name, call);
}

}  // namespace speed
}  // namespace esphome
