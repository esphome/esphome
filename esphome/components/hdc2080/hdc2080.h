#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome::hdc2080 {

class HDC2080Component final : public PollingComponent, public i2c::I2CDevice {
  SUB_SENSOR(temperature)
  SUB_SENSOR(humidity)

 public:
  /// Setup the sensor and check for connection.
  void setup() override;
  void dump_config() override;
  void update() override;
};

}  // namespace esphome::hdc2080
