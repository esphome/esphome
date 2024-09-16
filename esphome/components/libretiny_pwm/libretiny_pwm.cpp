#include "libretiny_pwm.h"
#include "esphome/core/log.h"

#ifdef USE_LIBRETINY

namespace esphome {
namespace libretiny_pwm {

static const char *const TAG = "libretiny.pwm";

void LibreTinyPWM::write_state(float state) {
  if (!this->initialized_) {
    ESP_LOGW(TAG, "LibreTinyPWM output hasn't been initialized yet!");
    return;
  }

  if (this->pin_->is_inverted())
    state = 1.0f - state;

  this->duty_ = state;
  const uint32_t max_duty = (uint32_t(1) << this->bit_depth_) - 1;
  const float duty_rounded = roundf(state * max_duty);
  auto duty = static_cast<uint32_t>(duty_rounded);

  analogWrite(this->pin_->get_pin(), duty);  // NOLINT
}

void LibreTinyPWM::setup() {
  this->update_frequency(this->frequency_);
  this->turn_off();
}

void LibreTinyPWM::dump_config() {
  ESP_LOGCONFIG(TAG, "PWM Output:");
  LOG_PIN("  Pin ", this->pin_);
  ESP_LOGCONFIG(TAG, "  Frequency: %.1f Hz", this->frequency_);
}

void LibreTinyPWM::update_frequency(float frequency) {
  this->frequency_ = frequency;
  // force changing the frequency by removing PWM mode
  this->pin_->pin_mode(gpio::FLAG_INPUT);
  analogWriteResolution(this->bit_depth_);  // NOLINT
  analogWriteFrequency(frequency);          // NOLINT
  this->initialized_ = true;
  // re-apply duty
  this->write_state(this->duty_);
}

}  // namespace libretiny_pwm
}  // namespace esphome

#endif
