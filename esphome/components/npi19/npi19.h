#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::npi19 {

/// This class implements support for the npi19 pressure and temperature i2c sensors.
class NPI19Component final : public PollingComponent, public i2c::I2CDevice {
  SUB_SENSOR(temperature)
  SUB_SENSOR(raw_pressure)

 public:
  void setup() override;
  void dump_config() override;
  void update() override;

 protected:
  i2c::ErrorCode read_(uint16_t &raw_temperature, uint16_t &raw_pressure);
};

}  // namespace esphome::npi19
