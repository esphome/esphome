#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace bno055 {

// BNO055 Register addresses
static const uint8_t BNO055_REGISTER_CHIP_ID = 0x00;
static const uint8_t BNO055_REGISTER_PAGE_ID = 0x07;
static const uint8_t BNO055_REGISTER_ACCEL_DATA_X_LSB = 0x08;
static const uint8_t BNO055_REGISTER_MAG_DATA_X_LSB = 0x0E;
static const uint8_t BNO055_REGISTER_GYRO_DATA_X_LSB = 0x14;
static const uint8_t BNO055_REGISTER_EULER_H_LSB = 0x1A;
static const uint8_t BNO055_REGISTER_QUATERNION_W_LSB = 0x20;
static const uint8_t BNO055_REGISTER_LINEAR_ACCEL_X_LSB = 0x28;
static const uint8_t BNO055_REGISTER_GRAVITY_X_LSB = 0x2E;
static const uint8_t BNO055_REGISTER_TEMP = 0x34;
static const uint8_t BNO055_REGISTER_SYS_TRIGGER = 0x3F;
static const uint8_t BNO055_REGISTER_PWR_MODE = 0x3E;
static const uint8_t BNO055_REGISTER_OPR_MODE = 0x3D;

// BNO055 Chip ID
static const uint8_t BNO055_CHIP_ID = 0xA0;

// Operation modes
enum BNO055OperationMode : uint8_t {
  OPERATION_MODE_CONFIG = 0x00,
  OPERATION_MODE_ACCONLY = 0x01,
  OPERATION_MODE_MAGONLY = 0x02,
  OPERATION_MODE_GYRONLY = 0x03,
  OPERATION_MODE_ACCMAG = 0x04,
  OPERATION_MODE_ACCGYRO = 0x05,
  OPERATION_MODE_MAGGYRO = 0x06,
  OPERATION_MODE_AMG = 0x07,
  OPERATION_MODE_IMUPLUS = 0x08,
  OPERATION_MODE_COMPASS = 0x09,
  OPERATION_MODE_M4G = 0x0A,
  OPERATION_MODE_NDOF_FMC_OFF = 0x0B,
  OPERATION_MODE_NDOF = 0x0C,
};

// Power modes
enum BNO055PowerMode : uint8_t {
  POWER_MODE_NORMAL = 0x00,
  POWER_MODE_LOWPOWER = 0x01,
  POWER_MODE_SUSPEND = 0x02,
};

// Data scaling factors from BNO055 datasheet
// Euler angles: 1 degree = 16 LSB (fusion output)
static const float BNO055_EULER_SCALE = 1.0f / 16.0f;
// Quaternion: 1.0 = 2^14 LSB
static const float BNO055_QUATERNION_SCALE = 1.0f / 16384.0f;
// Acceleration: 1 m/s^2 = 100 LSB
static const float BNO055_ACCEL_SCALE = 1.0f / 100.0f;
// Gyroscope: 1 degree/s = 16 LSB
static const float BNO055_GYRO_SCALE = 1.0f / 16.0f;
// Magnetometer: 1 uT = 16 LSB
static const float BNO055_MAG_SCALE = 1.0f / 16.0f;

class BNO055Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override;

  // Euler angle sensor setters
  void set_euler_heading_sensor(sensor::Sensor *sensor) { this->euler_heading_sensor_ = sensor; }
  void set_euler_roll_sensor(sensor::Sensor *sensor) { this->euler_roll_sensor_ = sensor; }
  void set_euler_pitch_sensor(sensor::Sensor *sensor) { this->euler_pitch_sensor_ = sensor; }

  // Quaternion sensor setters
  void set_quaternion_w_sensor(sensor::Sensor *sensor) { this->quaternion_w_sensor_ = sensor; }
  void set_quaternion_x_sensor(sensor::Sensor *sensor) { this->quaternion_x_sensor_ = sensor; }
  void set_quaternion_y_sensor(sensor::Sensor *sensor) { this->quaternion_y_sensor_ = sensor; }
  void set_quaternion_z_sensor(sensor::Sensor *sensor) { this->quaternion_z_sensor_ = sensor; }

  // Linear acceleration sensor setters
  void set_linear_accel_x_sensor(sensor::Sensor *sensor) { this->linear_accel_x_sensor_ = sensor; }
  void set_linear_accel_y_sensor(sensor::Sensor *sensor) { this->linear_accel_y_sensor_ = sensor; }
  void set_linear_accel_z_sensor(sensor::Sensor *sensor) { this->linear_accel_z_sensor_ = sensor; }

  // Gravity sensor setters
  void set_gravity_x_sensor(sensor::Sensor *sensor) { this->gravity_x_sensor_ = sensor; }
  void set_gravity_y_sensor(sensor::Sensor *sensor) { this->gravity_y_sensor_ = sensor; }
  void set_gravity_z_sensor(sensor::Sensor *sensor) { this->gravity_z_sensor_ = sensor; }

  // Raw accelerometer sensor setters
  void set_accel_x_sensor(sensor::Sensor *sensor) { this->accel_x_sensor_ = sensor; }
  void set_accel_y_sensor(sensor::Sensor *sensor) { this->accel_y_sensor_ = sensor; }
  void set_accel_z_sensor(sensor::Sensor *sensor) { this->accel_z_sensor_ = sensor; }

  // Gyroscope sensor setters
  void set_gyro_x_sensor(sensor::Sensor *sensor) { this->gyro_x_sensor_ = sensor; }
  void set_gyro_y_sensor(sensor::Sensor *sensor) { this->gyro_y_sensor_ = sensor; }
  void set_gyro_z_sensor(sensor::Sensor *sensor) { this->gyro_z_sensor_ = sensor; }

  // Magnetometer sensor setters
  void set_mag_x_sensor(sensor::Sensor *sensor) { this->mag_x_sensor_ = sensor; }
  void set_mag_y_sensor(sensor::Sensor *sensor) { this->mag_y_sensor_ = sensor; }
  void set_mag_z_sensor(sensor::Sensor *sensor) { this->mag_z_sensor_ = sensor; }

  // Temperature sensor setter
  void set_temperature_sensor(sensor::Sensor *sensor) { this->temperature_sensor_ = sensor; }

 protected:
  bool set_mode_(BNO055OperationMode mode);

  // Euler angle sensors
  sensor::Sensor *euler_heading_sensor_{nullptr};
  sensor::Sensor *euler_roll_sensor_{nullptr};
  sensor::Sensor *euler_pitch_sensor_{nullptr};

  // Quaternion sensors
  sensor::Sensor *quaternion_w_sensor_{nullptr};
  sensor::Sensor *quaternion_x_sensor_{nullptr};
  sensor::Sensor *quaternion_y_sensor_{nullptr};
  sensor::Sensor *quaternion_z_sensor_{nullptr};

  // Linear acceleration sensors
  sensor::Sensor *linear_accel_x_sensor_{nullptr};
  sensor::Sensor *linear_accel_y_sensor_{nullptr};
  sensor::Sensor *linear_accel_z_sensor_{nullptr};

  // Gravity sensors
  sensor::Sensor *gravity_x_sensor_{nullptr};
  sensor::Sensor *gravity_y_sensor_{nullptr};
  sensor::Sensor *gravity_z_sensor_{nullptr};

  // Raw accelerometer sensors
  sensor::Sensor *accel_x_sensor_{nullptr};
  sensor::Sensor *accel_y_sensor_{nullptr};
  sensor::Sensor *accel_z_sensor_{nullptr};

  // Gyroscope sensors
  sensor::Sensor *gyro_x_sensor_{nullptr};
  sensor::Sensor *gyro_y_sensor_{nullptr};
  sensor::Sensor *gyro_z_sensor_{nullptr};

  // Magnetometer sensors
  sensor::Sensor *mag_x_sensor_{nullptr};
  sensor::Sensor *mag_y_sensor_{nullptr};
  sensor::Sensor *mag_z_sensor_{nullptr};

  // Temperature sensor
  sensor::Sensor *temperature_sensor_{nullptr};
};

}  // namespace bno055
}  // namespace esphome
