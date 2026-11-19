#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::iaqcore {

class IAQCore final : public PollingComponent, public i2c::I2CDevice {
  SUB_SENSOR(co2)
  SUB_SENSOR(tvoc)

 public:
  void setup() override;
  void update() override;
  void dump_config() override;

 protected:
  void publish_nans_();
};

}  // namespace esphome::iaqcore
