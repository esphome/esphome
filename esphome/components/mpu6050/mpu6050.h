#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace mpu6050 {

const uint8_t MPU6050_REGISTER_WHO_AM_I = 0x75;
const uint8_t MPU6050_REGISTER_POWER_MANAGEMENT_1 = 0x6B;
const uint8_t MPU6050_REGISTER_INT_ENABLE = 0x38;
const uint8_t MPU6050_REGISTER_MOTION_THRESHOLD = 0x1F;
const uint8_t MPU6050_REGISTER_MOTION_DURATION = 0x20;
const uint8_t MPU6050_REGISTER_GYRO_CONFIG = 0x1B;
const uint8_t MPU6050_REGISTER_ACCEL_CONFIG = 0x1C;
const uint8_t MPU6050_REGISTER_ACCEL_XOUT_H = 0x3B;
const uint8_t MPU6050_REGISTER_INT_PIN_CFG = 0x37;
const uint8_t MPU6050_REGISTER_MOT_DETECT_CTRL = 0x69;
const uint8_t MPU6050_BIT_RESET = 7;
const uint8_t MPU6050_BIT_SLEEP_ENABLED = 6;
const uint8_t MPU6050_BIT_TEMPERATURE_DISABLED = 3;
const uint8_t MPU6050_BIT_MOTION_DET = 6;
const uint8_t MPU6050_CLOCK_SOURCE_X_GYRO = 0b001;
const uint8_t MPU6050_SCALE_2000_DPS = 0b11;
const float MPU6050_SCALE_DPS_PER_DIGIT_2000 = 0.060975f;
const uint8_t MPU6050_RANGE_2G = 0b00;
const float MPU6050_RANGE_PER_DIGIT_2G = 0.000061f;

const uint8_t MPU6050_BIT_MOT_EN = 6;

class MPU6050Component : public i2c::I2CDevice, public Component {
 public:
  void setup() override;
  void dump_config() override;

  float get_setup_priority() const override;

#ifdef USE_MPU6050_INTERRUPT
  void set_interrupt(uint8_t threshold, uint8_t duration);
#endif  // USE_MPU6050_INTERRUPT

 protected:
#ifdef USE_MPU6050_INTERRUPT
  uint8_t threshold_;
  uint8_t duration_;
#endif  // USE_MPU6050_INTERRUPT
};
;

}  // namespace mpu6050
}  // namespace esphome
