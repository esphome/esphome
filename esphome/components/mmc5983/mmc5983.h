#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::mmc5983 {

class MMC5983Component final : public PollingComponent, public i2c::I2CDevice {
  SUB_SENSOR(x)
  SUB_SENSOR(y)
  SUB_SENSOR(z)

 public:
  void update() override;
  void setup() override;
  void dump_config() override;
};

}  // namespace esphome::mmc5983
