#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::bmi160 {

class BMI160Component final : public PollingComponent, public i2c::I2CDevice {
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

 protected:
  void internal_setup_(int stage);
  /// Run one setup stage; false on an I2C failure.
  bool setup_stage_(int stage);
  bool setup_complete_{false};
};

}  // namespace esphome::bmi160
