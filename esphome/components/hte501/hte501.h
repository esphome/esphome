#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome::hte501 {

/// This class implements support for the hte501 of temperature i2c sensors.
class HTE501Component final : public PollingComponent, public i2c::I2CDevice {
  SUB_SENSOR(temperature)
  SUB_SENSOR(humidity)

 public:
  void setup() override;
  void dump_config() override;
  void update() override;
};

}  // namespace esphome::hte501
