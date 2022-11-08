#ifdef USE_RP2040

#include "rp2040_pwm.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/macros.h"

#include <PinNames.h>

namespace esphome {
namespace rp2040_pwm {

static const char *const TAG = "rp2040_pwm";

void RP2040PWM::setup() {
  ESP_LOGCONFIG(TAG, "Setting up RP2040 PWM Output...");
  this->pin_->setup();
  this->pwm_ = new mbed::PwmOut((PinName) this->pin_->get_pin());
  this->turn_off();
}
void RP2040PWM::dump_config() {
  ESP_LOGCONFIG(TAG, "RP2040 PWM:");
  LOG_PIN("  Pin: ", this->pin_);
  ESP_LOGCONFIG(TAG, "  Frequency: %.1f Hz", this->frequency_);
  LOG_FLOAT_OUTPUT(this);
}
void HOT RP2040PWM::write_state(float state) {
  this->last_output_ = state;

  // Also check pin inversion
  if (this->pin_->is_inverted()) {
    state = 1.0f - state;
  }

  auto total_time_us = static_cast<uint32_t>(roundf(1e6f / this->frequency_));

  this->pwm_->period_us(total_time_us);
  this->pwm_->write(state);
}

}  // namespace rp2040_pwm
}  // namespace esphome

#endif
