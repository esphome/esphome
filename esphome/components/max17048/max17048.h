#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::max17048 {

class MAX17048Component final : public PollingComponent, public i2c::I2CDevice {
  SUB_SENSOR(battery_voltage)
  SUB_SENSOR(battery_level)
  SUB_SENSOR(rate)

 public:
  void setup() override;
  void dump_config() override;
  void update() override;

 protected:
  /// Reads a register and reports the component warning state from the result
  bool read_register_(uint8_t reg, uint16_t &value);
};

}  // namespace esphome::max17048
