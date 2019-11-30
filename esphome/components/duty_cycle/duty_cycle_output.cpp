#include "duty_cycle_output.h"
#include "esphome/core/log.h"

namespace esphome {
namespace duty_cycle {

static const char *TAG = "output.duty_cycle";

void DutyCycleOutput::setup() {
  this->pin_->setup();
  this->turn_off();
}

void DutyCycleOutput::loop() {
  unsigned long now = millis();
  float scaled_state = this->state_ * this->period_;

  if (now - this->period_start_time_ > this->period_) {
    ESP_LOGD(TAG, "End of period. State: %f, Scaled state: %f", this->state_, scaled_state);
    this->period_start_time_ += this->period_;
  }

  if (scaled_state > now - this->period_start_time_) {
      this->pin_->digital_write(true);
  } else {
      this->pin_->digital_write(false);
  }
}

void DutyCycleOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "Time Proportioning Output:");
  LOG_PIN("  Pin: ", this->pin_);
  ESP_LOGCONFIG(TAG, "  Period: %d ms", this->period_);
  LOG_FLOAT_OUTPUT(this);
}

void DutyCycleOutput::write_state(float state) {
  ESP_LOGI(TAG, "write_state: new state: %f", state);
  this->state_ = state;
}

}  // namespace duty_cycle
}  // namespace esphome