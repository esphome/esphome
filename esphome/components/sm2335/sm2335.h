#pragma once

#include "esphome/components/sm10bit_base/sm10bit_base.h"

namespace esphome::sm2335 {

class SM2335 final : public sm10bit_base::Sm10BitBase {
 public:
  SM2335(GPIOPin *data_pin, GPIOPin *clock_pin, uint8_t max_power_color_channels, uint8_t max_power_white_channels)
      : Sm10BitBase(data_pin, clock_pin, MODEL_ID, max_power_color_channels, max_power_white_channels) {}

  void dump_config() override;

 protected:
  static constexpr uint8_t MODEL_ID = 0xC0;
};

}  // namespace esphome::sm2335
