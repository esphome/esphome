#pragma once

#include "../waveshare_io_ch32v003.h"
#include "esphome/components/output/float_output.h"

namespace esphome::waveshare_io_ch32v003 {

class WaveshareIOCH32V003Output : public output::FloatOutput, public Parented<WaveshareIOCH32V003Component> {
 public:
  void set_pwm_safe_range(uint8_t min_value, uint8_t max_value) {
    this->pwm_min_value_ = min_value;
    this->pwm_max_value_ = max_value;
  }

 protected:
  void write_state(float state) override;

  uint8_t pwm_min_value_{1};
  uint8_t pwm_max_value_{247};
};

}  // namespace esphome::waveshare_io_ch32v003
