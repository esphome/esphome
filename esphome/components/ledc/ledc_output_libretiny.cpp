#include "ledc_output.h"
#include "esphome/core/log.h"

#ifdef USE_LIBRETINY

namespace esphome {
namespace ledc {

static const char *const TAG = "ledc.output";

uint8_t next_ledc_channel = 0;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void LEDCOutput::write_state(float state) {
  if (!initialized_) {
    ESP_LOGW(TAG, "LEDC output hasn't been initialized yet!");
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

void LEDCOutput::setup() {
  this->update_frequency(this->frequency_);
  this->turn_off();
}

void LEDCOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "PWM Output:");
  LOG_PIN("  Pin ", this->pin_);
  ESP_LOGCONFIG(TAG, "  Frequency: %.1f Hz", this->frequency_);
}

void LEDCOutput::update_frequency(float frequency) {
  this->bit_depth_ = 10;
  this->frequency_ = frequency;
  // force changing the frequency by removing PWM mode
  this->pin_->pin_mode(gpio::FLAG_INPUT);
  analogWriteResolution(10);        // NOLINT
  analogWriteFrequency(frequency);  // NOLINT
  initialized_ = true;
  // re-apply duty
  this->write_state(this->duty_);
}

}  // namespace ledc
}  // namespace esphome

#endif
