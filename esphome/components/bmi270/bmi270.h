#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace bmi270 {

class BMI270Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;

  void update() override;

  float get_setup_priority() const override;

  void set_accel_x_sensor(sensor::Sensor *accel_x_sensor) { this->accel_x_sensor_ = accel_x_sensor; }
  void set_accel_y_sensor(sensor::Sensor *accel_y_sensor) { this->accel_y_sensor_ = accel_y_sensor; }
  void set_accel_z_sensor(sensor::Sensor *accel_z_sensor) { this->accel_z_sensor_ = accel_z_sensor; }
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { this->temperature_sensor_ = temperature_sensor; }
  void set_gyro_x_sensor(sensor::Sensor *gyro_x_sensor) { this->gyro_x_sensor_ = gyro_x_sensor; }
  void set_gyro_y_sensor(sensor::Sensor *gyro_y_sensor) { this->gyro_y_sensor_ = gyro_y_sensor; }
  void set_gyro_z_sensor(sensor::Sensor *gyro_z_sensor) { this->gyro_z_sensor_ = gyro_z_sensor; }

 protected:
  enum class SetupState : uint8_t {
    SOFT_RESET = 0,
    CHECK_CHIP_ID,
    PREPARE_CONFIG_UPLOAD,
    UPLOAD_CONFIG,
    COMPLETE_INIT,
    CHECK_STATUS,
    CONFIGURE_SENSORS,
    DONE,
  };

  sensor::Sensor *accel_x_sensor_{nullptr};
  sensor::Sensor *accel_y_sensor_{nullptr};
  sensor::Sensor *accel_z_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *gyro_x_sensor_{nullptr};
  sensor::Sensor *gyro_y_sensor_{nullptr};
  sensor::Sensor *gyro_z_sensor_{nullptr};

  void internal_setup_(SetupState state);
  bool setup_complete_{false};

  /** reads `len` 16-bit little-endian integers from the given i2c register */
  i2c::ErrorCode read_le_int16_(uint8_t reg, int16_t *value, uint8_t len);
};

}  // namespace bmi270
}  // namespace esphome
