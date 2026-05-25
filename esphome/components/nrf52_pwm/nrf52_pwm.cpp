#ifdef USE_NRF52

#include "nrf52_pwm.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cinttypes>
#include <cmath>

namespace esphome::nrf52_pwm {

// Zephyr PWM API: period and pulse widths are passed in nanoseconds to pwm_set().
// https://docs.zephyrproject.org/latest/doxygen/html/group__pwm__interface.html
static const char *const TAG = "nrf52_pwm";

void Nrf52PWMOutput::setup() {
  if (this->dev_ == nullptr) {
    ESP_LOGE(TAG, "PWM device is not configured");
    this->mark_failed();
    return;
  }
  if (!device_is_ready(this->dev_)) {
    ESP_LOGE(TAG, "PWM device is not ready");
    this->mark_failed();
    return;
  }
  this->turn_off();
}

void Nrf52PWMOutput::dump_config() {
  ESP_LOGCONFIG(TAG,
                "nRF52 PWM:\n"
                "  Frequency: %.1f Hz\n"
                "  Channel: %" PRIu32,
                this->frequency_, this->channel_);
  LOG_PIN("  Pin: ", this->pin_);
  LOG_FLOAT_OUTPUT(this);
  if (!this->runtime_frequency_mutable_) {
    ESP_LOGCONFIG(TAG, "  Runtime frequency changes: disabled for shared PWM peripheral");
  }
}

uint32_t Nrf52PWMOutput::period_ns_() const { return static_cast<uint32_t>(roundf(1000000000.0f / this->frequency_)); }

bool Nrf52PWMOutput::write_pwm_() {
  const uint32_t period_ns = this->period_ns_();
  const uint32_t pulse_ns = static_cast<uint32_t>(roundf(static_cast<float>(period_ns) * this->last_output_));
  pwm_flags_t flags = 0;
  if (this->pin_ != nullptr && this->pin_->is_inverted()) {
    flags |= PWM_POLARITY_INVERTED;
  }
  // Zephyr pwm_nrfx drives pulse 0 and pulse >= period as constant pin states, and stops the PWM peripheral
  // when every channel on it is constant. This matches LEDC-style stop behavior for 0% and 100% outputs.
  int ret = pwm_set(this->dev_, this->channel_, period_ns, pulse_ns, flags);
  if (ret != 0) {
    ESP_LOGE(TAG, "pwm_set() failed for channel %" PRIu32 ": %d", this->channel_, ret);
    return false;
  }
  return true;
}

void Nrf52PWMOutput::write_state(float state) {
  this->last_output_ = state;
  this->write_pwm_();
}

void Nrf52PWMOutput::update_frequency(float frequency) {
  if (!this->runtime_frequency_mutable_) {
    ESP_LOGW(TAG, "Ignoring runtime frequency change on shared PWM peripheral");
    return;
  }
  this->frequency_ = frequency;
  this->write_pwm_();
}

}  // namespace esphome::nrf52_pwm

#endif  // USE_NRF52
