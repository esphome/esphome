#include "speed_fan.h"
#include "esphome/components/fan/fan_helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace speed {

static const char *const TAG = "speed.fan";

void SpeedFan::dump_config() {
  ESP_LOGCONFIG(TAG, "Fan '%s':", this->fan_->get_name().c_str());
  if (this->fan_->get_traits().supports_oscillation()) {
    ESP_LOGCONFIG(TAG, "  Oscillation: YES");
  }
  if (this->fan_->get_traits().supports_direction()) {
    ESP_LOGCONFIG(TAG, "  Direction: YES");
  }
}
void SpeedFan::setup() {
  auto traits = fan::FanTraits(this->oscillating_ != nullptr, true, this->direction_ != nullptr, this->speed_count_);
  this->fan_->set_traits(traits);
  this->fan_->add_on_state_callback([this]() { this->next_update_ = true; });
}
void SpeedFan::loop() {
  if (!this->next_update_) {
    return;
  }
  this->next_update_ = false;

  {
    float speed = 0.0f;
    if (this->fan_->state) {
      speed = static_cast<float>(this->fan_->speed) / static_cast<float>(this->speed_count_);
    }
    ESP_LOGD(TAG, "Setting speed: %.2f", speed);
    this->output_->set_level(speed);
  }

  if (this->oscillating_ != nullptr) {
    bool enable = this->fan_->oscillating;
    if (enable) {
      this->oscillating_->turn_on();
    } else {
      this->oscillating_->turn_off();
    }
    ESP_LOGD(TAG, "Setting oscillation: %s", ONOFF(enable));
  }

  if (this->direction_ != nullptr) {
    bool enable = this->fan_->direction == fan::FAN_DIRECTION_REVERSE;
    if (enable) {
      this->direction_->turn_on();
    } else {
      this->direction_->turn_off();
    }
    ESP_LOGD(TAG, "Setting reverse direction: %s", ONOFF(enable));
  }
}

// We need a higher priority than the FanState component to make sure that the traits are set
// when that component sets itself up.
float SpeedFan::get_setup_priority() const { return fan_->get_setup_priority() + 1.0f; }

}  // namespace speed
}  // namespace esphome
