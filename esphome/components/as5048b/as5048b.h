#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace as5048b {

class AS5048bComponent : public PollingComponent, public i2c::I2CDevice, public sensor::Sensor {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;

  float get_setup_priority() const override;

 protected:
  void read_angle_();
};

}  // namespace as5048b
}  // namespace esphome
