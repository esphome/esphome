#include "waveshare_io_ch32v003_output.h"

namespace esphome {
namespace waveshare_io_ch32v003 {

void WaveshareIOCH32V003Output::write_state(float state) {
  this->parent_->set_pwm_value(static_cast<uint8_t>(state * 255.0f));
}

}  // namespace waveshare_io_ch32v003
}  // namespace esphome
