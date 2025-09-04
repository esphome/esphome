#include "waveshare_io_ch32v003_output.h"
#include <algorithm>

namespace esphome {
namespace waveshare_io_ch32v003 {

void WaveshareIOCH32V003Output::write_state(float state) {
  uint8_t pwm_value = static_cast<uint8_t>(state * 255.0f);
  pwm_value = std::clamp(pwm_value, this->pwm_min_value_, this->pwm_max_value_);
  this->parent_->set_pwm_value(pwm_value);
}

}  // namespace waveshare_io_ch32v003
}  // namespace esphome
