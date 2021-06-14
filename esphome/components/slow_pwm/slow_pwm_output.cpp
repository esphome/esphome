#include "slow_pwm_output.h"
#include "esphome/core/log.h"

namespace esphome {
namespace slow_pwm {

static const char *const TAG = "output.slow_pwm";

void SlowPWMOutput::setup() {
  this->pin_->setup();
  this->turn_off();
}

void SlowPWMOutput::loop() {
  uint32_t now = millis();
  float scaled_state = this->state_ * this->period_;

  if (now - this->period_start_time_ > this->period_) {
    ESP_LOGVV(TAG, "End of period. State: %f, Scaled state: %f", this->state_, scaled_state);
    this->period_start_time_ += this->period_;
  }

  if (scaled_state > now - this->period_start_time_) {
    this->pin_->digital_write(true);
  } else {
    this->pin_->digital_write(false);
  }
}

void SlowPWMOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "Slow PWM Output:");
  LOG_PIN("  Pin: ", this->pin_);
  ESP_LOGCONFIG(TAG, "  Period: %d ms", this->period_);
  LOG_FLOAT_OUTPUT(this);
}

void SlowPWMOutput::write_state(float state) { this->state_ = state; }

}  // namespace slow_pwm
}  // namespace esphome
