#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::bmi160 {

class BMI160Component final : public PollingComponent, public i2c::I2CDevice {
 protected:
  // First so it packs into the padding after I2CDevice::address_
  bool setup_complete_{false};

  SUB_SENSOR(accel_x)
  SUB_SENSOR(accel_y)
  SUB_SENSOR(accel_z)
  SUB_SENSOR(temperature)
  SUB_SENSOR(gyro_x)
  SUB_SENSOR(gyro_y)
  SUB_SENSOR(gyro_z)

 public:
  void setup() override;
  void dump_config() override;

  void update() override;
};

}  // namespace esphome::bmi160
