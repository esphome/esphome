#include "slow_pwm_output.h"
#include "esphome/core/log.h"

namespace esphome {
namespace slow_pwm {

static const char *const TAG = "output.slow_pwm";

void SlowPWMOutput::setup() {
  if (this->pin_)
    this->pin_->setup();
  this->turn_off();
}

/// turn on/off the configured output
void SlowPWMOutput::set_output_state_(bool new_state) {
  if (this->pin_) {
    this->pin_->digital_write(new_state);
  }
  if (new_state != current_state_) {
    if (this->state_change_trigger_) {
      this->state_change_trigger_->trigger(new_state);
    }
    if (new_state) {
      if (this->turn_on_trigger_)
        this->turn_on_trigger_->trigger();
    } else {
      if (this->turn_off_trigger_)
        this->turn_off_trigger_->trigger();
    }
    current_state_ = new_state;
  }
}

void SlowPWMOutput::loop() {
  uint32_t now = millis();
  float scaled_state = this->state_ * this->period_;

  if (now - this->period_start_time_ >= this->period_) {
    ESP_LOGVV(TAG, "End of period. State: %f, Scaled state: %f", this->state_, scaled_state);
    this->period_start_time_ += this->period_;
  }

  if (scaled_state > now - this->period_start_time_) {
    this->set_output_state_(true);
  } else {
    this->set_output_state_(false);
  }
}

void SlowPWMOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "Slow PWM Output:");
  LOG_PIN("  Pin: ", this->pin_);
  if (this->state_change_trigger_)
    ESP_LOGCONFIG(TAG, "  State change automation configured");
  if (this->turn_on_trigger_)
    ESP_LOGCONFIG(TAG, "  Turn on automation configured");
  if (this->turn_off_trigger_)
    ESP_LOGCONFIG(TAG, "  Turn off automation configured");
  ESP_LOGCONFIG(TAG, "  Period: %d ms", this->period_);
  LOG_FLOAT_OUTPUT(this);
}

}  // namespace slow_pwm
}  // namespace esphome
