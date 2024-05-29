#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace mpu6050 {

class MPU6050Component : public i2c::I2CDevice, public Component {
 public:
  void setup() override;
  void dump_config() override;

  float get_setup_priority() const override;

 protected:
  uint8_t thresold;
};
;

}  // namespace mpu6050
}  // namespace esphome
