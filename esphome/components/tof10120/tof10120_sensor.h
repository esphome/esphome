#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace tof10120 {

class TOF10120Sensor : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;

  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void update() override;
};
}  // namespace tof10120
}  // namespace esphome
