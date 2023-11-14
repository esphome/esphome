#ifdef USE_RP2040

#include "rp2040_pwm.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/macros.h"

#include <hardware/clocks.h>
#include <hardware/gpio.h>
#include <hardware/pwm.h>
#include <cmath>

namespace esphome {
namespace rp2040_pwm {

static const char *const TAG = "rp2040_pwm";

void RP2040PWM::setup() {
  ESP_LOGCONFIG(TAG, "Setting up RP2040 PWM Output...");

  this->setup_pwm_();
}

void RP2040PWM::setup_pwm_() {
  pwm_config config = pwm_get_default_config();

  uint32_t clock = clock_get_hz(clk_sys);
  float divider = ceil(clock / (4096 * this->frequency_)) / 16.0f;
  if (divider < 1.0f) {
    divider = 1.0f;
  }
  uint16_t wrap = clock / divider / this->frequency_ - 1;
  this->wrap_ = wrap;
  ESP_LOGD(TAG, "divider=%.5f, wrap=%d, clock=%d", divider, wrap, clock);

  pwm_config_set_clkdiv(&config, divider);
  pwm_config_set_wrap(&config, wrap);
  pwm_init(pwm_gpio_to_slice_num(this->pin_->get_pin()), &config, true);
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

  if (this->frequency_changed_) {
    this->setup_pwm_();
    this->frequency_changed_ = false;
  }

  gpio_set_function(this->pin_->get_pin(), GPIO_FUNC_PWM);
  pwm_set_gpio_level(this->pin_->get_pin(), state * this->wrap_);
}

}  // namespace rp2040_pwm
}  // namespace esphome

#endif
