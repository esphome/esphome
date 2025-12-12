#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace gl_r01_i2c {

class GLR01I2CComponent : public sensor::Sensor, public i2c::I2CDevice, public PollingComponent {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;

 protected:
  void read_distance_();
  uint16_t version_{0};
};

}  // namespace gl_r01_i2c
}  // namespace esphome
