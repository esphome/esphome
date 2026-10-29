#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::tem3200 {

/// This class implements support for the tem3200 pressure and temperature i2c sensors.
class TEM3200Component final : public PollingComponent, public i2c::I2CDevice {
  SUB_SENSOR(temperature)
  SUB_SENSOR(raw_pressure)

 public:
  void setup() override;
  void dump_config() override;
  void update() override;

 protected:
  i2c::ErrorCode read_(uint8_t &status, uint16_t &raw_temperature, uint16_t &raw_pressure);
};

}  // namespace esphome::tem3200
