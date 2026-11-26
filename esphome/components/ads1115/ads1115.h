#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"

namespace esphome::ads1115 {

class ADS1115Component final : public Component, public i2c::I2CDevice {
 public:
  explicit ADS1115Component(bool continuous_mode) : continuous_mode_(continuous_mode) {}
  void setup() override;
  void dump_config() override;

  /// Returns volts for `config_bits` (multiplexer, gain and data rate fields of the config register).
  float request_measurement(uint16_t config_bits, uint8_t conversion_delay_ms);

 protected:
  uint16_t prev_config_{0};
  const bool continuous_mode_;
};

}  // namespace esphome::ads1115
