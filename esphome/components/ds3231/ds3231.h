#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::ds3231 {

/// Hub for a single DS3231 real-time clock. Owns all register access; the platforms talk to the
/// chip through this class.
class DS3231Component final : public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;

  bool read_temperature(float &out);
  void force_temperature_conversion();

 protected:
  static constexpr uint8_t CONTROL_INTCN = 0x04;
  static constexpr uint8_t CONTROL_CONV = 0x20;
  static constexpr uint8_t CONTROL_EOSC = 0x80;

  static constexpr uint8_t STATUS_BSY = 0x04;

  bool read_control_status_();
  bool write_control_();

  uint8_t control_reg_{0};
  uint8_t status_reg_{0};
};

}  // namespace esphome::ds3231
