#include "hbridge_fan.h"
#include "esphome/core/log.h"

namespace esphome {
namespace hbridge {

static const char *const TAG = "fan.hbridge";

void HBridgeFan::set_hbridge_levels_(float a_level, float b_level) {
  this->pin_a_->set_level(a_level);
  this->pin_b_->set_level(b_level);
  ESP_LOGD(TAG, "Setting speed: a: %.2f, b: %.2f", a_level, b_level);
}

// constant IN1/IN2, PWM on EN => power control, fast current decay
// constant IN1/EN, PWM on IN2 => power control, slow current decay
void HBridgeFan::set_hbridge_levels_(float a_level, float b_level, float enable) {
  this->pin_a_->set_level(a_level);
  this->pin_b_->set_level(b_level);
  this->enable_->set_level(enable);
  ESP_LOGD(TAG, "Setting speed: a: %.2f, b: %.2f, enable: %.2f", a_level, b_level, enable);
}

fan::FanCall HBridgeFan::brake() {
  ESP_LOGD(TAG, "Braking");
  (this->enable_ == nullptr) ? this->set_hbridge_levels_(1.0f, 1.0f) : this->set_hbridge_levels_(1.0f, 1.0f, 1.0f);
  return this->make_call().set_state(false);
}

void HBridgeFan::setup() {
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->apply(*this);
    this->write_state_();
  }

  // Construct traits
  this->traits_ = fan::FanTraits(this->oscillating_ != nullptr, true, true, this->speed_count_);

  // Add all presets to traits
  std::vector<std::string> keys;
  for (auto const &kv : this->preset_modes_)
    keys.push_back(kv.first);

  this->traits_.set_supported_preset_modes(keys);
}

void HBridgeFan::dump_config() {
  LOG_FAN("", "H-Bridge Fan", this);
  if (this->decay_mode_ == DECAY_MODE_SLOW) {
    ESP_LOGCONFIG(TAG, "  Decay Mode: Slow");
  } else {
    ESP_LOGCONFIG(TAG, "  Decay Mode: Fast");
  }
}

void HBridgeFan::control(const fan::FanCall &call) {
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

void HBridgeFan::write_state_() {
  float speed = this->state ? static_cast<float>(this->speed) / static_cast<float>(this->speed_count_) : 0.0f;
  if (speed == 0.0f) {  // off means idle
    (this->enable_ == nullptr) ? this->set_hbridge_levels_(speed, speed)
                               : this->set_hbridge_levels_(speed, speed, speed);
  } else if (this->direction == fan::FanDirection::FORWARD) {
    if (this->decay_mode_ == DECAY_MODE_SLOW) {
      (this->enable_ == nullptr) ? this->set_hbridge_levels_(1.0f - speed, 1.0f)
                                 : this->set_hbridge_levels_(1.0f - speed, 1.0f, 1.0f);
    } else {  // DECAY_MODE_FAST
      (this->enable_ == nullptr) ? this->set_hbridge_levels_(0.0f, speed)
                                 : this->set_hbridge_levels_(0.0f, 1.0f, speed);
    }
  } else {  // fan::FAN_DIRECTION_REVERSE
    if (this->decay_mode_ == DECAY_MODE_SLOW) {
      (this->enable_ == nullptr) ? this->set_hbridge_levels_(1.0f, 1.0f - speed)
                                 : this->set_hbridge_levels_(1.0f, 1.0f - speed, 1.0f);
    } else {  // DECAY_MODE_FAST
      (this->enable_ == nullptr) ? this->set_hbridge_levels_(speed, 0.0f)
                                 : this->set_hbridge_levels_(1.0f, 0.0f, speed);
    }
  }

  if (this->oscillating_ != nullptr)
    this->oscillating_->set_state(this->oscillating);
}

void HBridgeFan::add_preset_mode(const std::string &name, optional<int> speed, optional<fan::FanDirection> direction) {
  auto call = this->make_call();

  call.set_preset_mode(name);

  if (speed)
    call.set_speed(*speed);

  if (direction)
    call.set_direction(static_cast<fan::FanDirection>(*direction));

  this->preset_modes_.emplace(name, call);
}

}  // namespace hbridge
}  // namespace esphome
