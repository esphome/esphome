#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::ee895 {

/// This class implements support for the ee895 of temperature i2c sensors.
class EE895Component final : public PollingComponent, public i2c::I2CDevice {
  SUB_SENSOR(co2)
  SUB_SENSOR(temperature)
  SUB_SENSOR(pressure)

 public:
  void setup() override;
  void dump_config() override;
  void update() override;

 protected:
  void write_command_(uint16_t addr, uint16_t reg_cnt);
  float read_float_();
  uint16_t calc_crc16_(const uint8_t buf[], uint8_t len);
};

}  // namespace esphome::ee895
