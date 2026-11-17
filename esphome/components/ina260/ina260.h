#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::ina260 {

class INA260Component final : public PollingComponent, public i2c::I2CDevice {
  SUB_SENSOR(bus_voltage)
  SUB_SENSOR(current)
  SUB_SENSOR(power)

 public:
  void setup() override;
  void dump_config() override;
  void update() override;

 protected:
  uint16_t manufacture_id_{0};
  uint16_t device_id_{0};
};

}  // namespace esphome::ina260
