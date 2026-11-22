#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::gy39 {

class GY39 final : public i2c::I2CDevice, public PollingComponent {
 public:
  SUB_SENSOR(illuminance)
  SUB_SENSOR(temperature)
  SUB_SENSOR(pressure)
  SUB_SENSOR(humidity)
  SUB_SENSOR(altitude)

  void setup() override;
  void update() override;
  void dump_config() override;
};

}  // namespace esphome::gy39
