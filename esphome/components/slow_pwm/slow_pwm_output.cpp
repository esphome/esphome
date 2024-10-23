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
    if (this->pin_) {
      ESP_LOGV(TAG, "Switching output pin %s to %s", this->pin_->dump_summary().c_str(), ONOFF(new_state));
    } else {
      ESP_LOGV(TAG, "Switching to %s", ONOFF(new_state));
    }

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
  float scaled_state = this->state_ * this->current_period_;

  if (this->state_ > 0 && (!this->max_period_ || this->current_period_ < this->max_period_) &&
      (scaled_state - (float) this->min_time_on_) < -1.0) {  // allow 1ms of floating point error
    this->current_period_ = (unsigned int) ((float) this->min_time_on_ / this->state_);
    ESP_LOGVV(TAG, "Current cycle extended to %d ms to prevent on time of just %.0f ms", this->current_period_,
              scaled_state);

    scaled_state = (float) this->min_time_on_;
  }

  if (this->state_ < 1.0 && (!this->max_period_ || this->current_period_ < this->max_period_) &&
      ((float) this->current_period_ - scaled_state - (float) this->min_time_off_) <
          -1.0) {  // allow 1ms of floating point error
    float required_time_off = (float) this->current_period_ - scaled_state;
    this->current_period_ = (unsigned int) ((float) this->min_time_off_ / (1.0 - this->state_));
    ESP_LOGVV(TAG, "Current cycle extended to %d ms to prevent off time of just %.0f ms", this->current_period_,
              required_time_off);
  }

  if (this->max_period_ && this->current_period_ > this->max_period_) {
    this->current_period_ = this->max_period_;
    ESP_LOGVV(TAG, "Current cycle reduced to %d ms to obey max period", this->current_period_);
  }

  if (this->state_ == 0) {
    scaled_state = 0;
  } else if (this->state_ == 1) {
    scaled_state = (float) this->current_period_;
  } else {
    scaled_state = std::min(std::max((float) this->min_time_on_, this->state_ * this->current_period_),
                            (float) this->current_period_ - this->min_time_off_);
  }

  if (now - this->period_start_time_ >= this->current_period_) {
    ESP_LOGVV(TAG, "End of period (%d ms). State: %f, Scaled state: %f", this->current_period_, this->state_,
              scaled_state);
    this->period_start_time_ += this->current_period_;
    this->current_period_ = this->period_;
  }

  this->set_output_state_(scaled_state > now - this->period_start_time_);
}

void SlowPWMOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "Slow PWM Output:");
  LOG_PIN("  Pin: ", this->pin_);
  if (this->state_change_trigger_) {
    ESP_LOGCONFIG(TAG, "  State change automation configured");
  }
  if (this->turn_on_trigger_) {
    ESP_LOGCONFIG(TAG, "  Turn on automation configured");
  }
  if (this->turn_off_trigger_) {
    ESP_LOGCONFIG(TAG, "  Turn off automation configured");
  }
  ESP_LOGCONFIG(TAG, "  Period: %d ms", this->period_);
  ESP_LOGCONFIG(TAG, "  Restart cycle on state change: %s", YESNO(this->restart_cycle_on_state_change_));
  ESP_LOGCONFIG(TAG, "  Minimum time on: %d ms", this->min_time_on_);
  ESP_LOGCONFIG(TAG, "  Minimum time off: %d ms", this->min_time_off_);
  ESP_LOGCONFIG(TAG, "  Maximum period length: %d ms", this->max_period_);
  LOG_FLOAT_OUTPUT(this);
}

void SlowPWMOutput::write_state(float state) {
  this->state_ = state;
  if (this->restart_cycle_on_state_change_)
    this->restart_cycle();
}

}  // namespace slow_pwm
}  // namespace esphome
