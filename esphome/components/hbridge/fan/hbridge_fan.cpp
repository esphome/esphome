#include "hbridge_fan.h"
#include "esphome/components/fan/fan_helpers.h"
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

fan::FanStateCall HBridgeFan::brake() {
  ESP_LOGD(TAG, "Braking");
  (this->enable_ == nullptr) ? this->set_hbridge_levels_(1.0f, 1.0f) : this->set_hbridge_levels_(1.0f, 1.0f, 1.0f);
  return this->make_call().set_state(false);
}

void HBridgeFan::dump_config() {
  ESP_LOGCONFIG(TAG, "Fan '%s':", this->get_name().c_str());
  if (this->get_traits().supports_oscillation()) {
    ESP_LOGCONFIG(TAG, "  Oscillation: YES");
  }
  if (this->get_traits().supports_direction()) {
    ESP_LOGCONFIG(TAG, "  Direction: YES");
  }
  if (this->decay_mode_ == DECAY_MODE_SLOW) {
    ESP_LOGCONFIG(TAG, "  Decay Mode: Slow");
  } else {
    ESP_LOGCONFIG(TAG, "  Decay Mode: Fast");
  }
}
void HBridgeFan::setup() {
  auto traits = fan::FanTraits(this->oscillating_ != nullptr, true, true, this->speed_count_);
  this->set_traits(traits);
  this->add_on_state_callback([this]() { this->next_update_ = true; });
}
void HBridgeFan::loop() {
  if (!this->next_update_) {
    return;
  }
  this->next_update_ = false;

  float speed = 0.0f;
  if (this->state) {
    speed = static_cast<float>(this->speed) / static_cast<float>(this->speed_count_);
  }
  if (speed == 0.0f) {  // off means idle
    (this->enable_ == nullptr) ? this->set_hbridge_levels_(speed, speed)
                               : this->set_hbridge_levels_(speed, speed, speed);
    return;
  }
  if (this->direction == fan::FAN_DIRECTION_FORWARD) {
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
}

}  // namespace hbridge
}  // namespace esphome
