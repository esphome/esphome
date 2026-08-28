#include "waveshare_io_ch32v003_output.h"
#include "esphome/core/log.h"
#include <algorithm>

namespace esphome::waveshare_io_ch32v003 {

static const char *const TAG = "waveshare_io_ch32v003.output";

void WaveshareIOCH32V003Output::write_state(float state) {
  uint8_t pwm_value = static_cast<uint8_t>(state * 255.0f);
  uint8_t final_pwm_value = std::clamp(pwm_value, this->pwm_min_value_, this->pwm_max_value_);
  if (final_pwm_value != pwm_value) {
    ESP_LOGVV(TAG, "Clamping PWM value %u to safe range [%u, %u]", pwm_value, this->pwm_min_value_,
              this->pwm_max_value_);
  }
  this->parent_->set_pwm_value(final_pwm_value);
}

}  // namespace esphome::waveshare_io_ch32v003
