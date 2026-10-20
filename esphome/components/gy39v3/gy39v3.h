#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::gy39v3 {

class GY39v3 : public i2c::I2CDevice, public PollingComponent {
 public:
  static const char TAG[];
  SUB_SENSOR(illuminance)
  SUB_SENSOR(temperature)
  SUB_SENSOR(pressure)
  SUB_SENSOR(humidity)
  SUB_SENSOR(elevation)

  void setup() override;
  void update() override;
  void dump_config() override;
};

}  // namespace esphome::gy39v3
