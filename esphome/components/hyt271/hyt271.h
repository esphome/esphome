#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::hyt271 {

class HYT271Component final : public PollingComponent, public i2c::I2CDevice {
  SUB_SENSOR(temperature)
  SUB_SENSOR(humidity)

 public:
  void dump_config() override;
  /// Update the sensor values (temperature+humidity).
  void update() override;
};

}  // namespace esphome::hyt271
