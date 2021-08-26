#include "hbridge_fan.h"
#include "esphome/components/fan/fan_helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace hbridge {

static const char *const TAG = "speed.fan";

void HBridgeFan::dump_config() {
  ESP_LOGCONFIG(TAG, "Fan '%s':", this->fan_->get_name().c_str());
  if (this->fan_->get_traits().supports_oscillation()) {
    ESP_LOGCONFIG(TAG, "  Oscillation: YES");
  }
  if (this->fan_->get_traits().supports_direction()) {
    ESP_LOGCONFIG(TAG, "  Direction: YES");
  }
}
void HBridgeFan::setup() {
  auto traits = fan::FanTraits(this->oscillating_ != nullptr, true, true, this->speed_count_);
  this->fan_->set_traits(traits);
  this->fan_->add_on_state_callback([this]() { this->next_update_ = true; });
}
void HBridgeFan::loop() {
  if (!this->next_update_) {
    return;
  }
  this->next_update_ = false;

  float speed = 0.0f;
  if (this->fan_->state) {
    speed = static_cast<float>(this->fan_->speed) / static_cast<float>(this->speed_count_);
  }
  if (speed == 0.0f) {  // off means idle
    this->pin_a_->set_level(speed);
    this->pin_b_->set_level(speed);
    ESP_LOGD(TAG, "Setting speed: a: %.2f, b: %.2f", speed, speed);
    return;
  }
  if (this->fan_->direction == fan::FAN_DIRECTION_FORWARD) {
    if (this->decay_mode_ == DECAY_MODE_SLOW) {
      this->pin_a_->set_level(1.0f - speed);
      this->pin_b_->set_level(1.0f);
      ESP_LOGD(TAG, "Decay Mode: slow");
      ESP_LOGD(TAG, "Setting speed: a: %.2f, b: %.2f", 1.0f - speed, 1.0f);
    } else {  // DECAY_MODE_FAST
      this->pin_a_->set_level(0.0f);
      this->pin_b_->set_level(speed);
      ESP_LOGD(TAG, "Decay Mode: fast");
      ESP_LOGD(TAG, "Setting speed: a: %.2f, b: %.2f", 0.0f, speed);
    }
  } else {  // fan::FAN_DIRECTION_REVERSE
    if (this->decay_mode_ == DECAY_MODE_SLOW) {
      this->pin_a_->set_level(1.0f);
      this->pin_b_->set_level(1.0f - speed);
      ESP_LOGD(TAG, "Decay Mode: slow");
      ESP_LOGD(TAG, "Setting speed: a: %.2f, b: %.2f", 1.0f, 1.0f - speed);
    } else {  // DECAY_MODE_FAST
      this->pin_a_->set_level(speed);
      this->pin_b_->set_level(0.0f);
      ESP_LOGD(TAG, "Decay Mode: fast");
      ESP_LOGD(TAG, "Setting speed: a: %.2f, b: %.2f", speed, 0.0f);
    }
  }
}

}  // namespace hbridge
}  // namespace esphome
