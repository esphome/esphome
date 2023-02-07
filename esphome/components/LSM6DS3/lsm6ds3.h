#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace LSM6DS3 {

enum SensorType { ACCEL, GYRO, TEMP };

class LSM6DS3Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;

  void update() override;

  float get_setup_priority() const override;

  void set_accel_x_sensor(sensor::Sensor *accel_x_sensor) { accel_x_sensor_ = accel_x_sensor; }
  void set_accel_y_sensor(sensor::Sensor *accel_y_sensor) { accel_y_sensor_ = accel_y_sensor; }
  void set_accel_z_sensor(sensor::Sensor *accel_z_sensor) { accel_z_sensor_ = accel_z_sensor; }
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_gyro_x_sensor(sensor::Sensor *gyro_x_sensor) { gyro_x_sensor_ = gyro_x_sensor; }
  void set_gyro_y_sensor(sensor::Sensor *gyro_y_sensor) { gyro_y_sensor_ = gyro_y_sensor; }
  void set_gyro_z_sensor(sensor::Sensor *gyro_z_sensor) { gyro_z_sensor_ = gyro_z_sensor; }

  void set_high_perf(bool high_perf) { this->high_perf_ = high_perf; }
  void set_sample_accl_rate(uint16_t sample_rate) { this->sample_accl_rate_ = sample_rate; }
  void set_sample_gyro_rate(uint16_t sample_rate) { this->sample_gyro_rate_ = sample_rate; }
  void set_power_save(bool power_save) { this->_power_save = power_save; }

 protected:
  sensor::Sensor *accel_x_sensor_{nullptr};
  sensor::Sensor *accel_y_sensor_{nullptr};
  sensor::Sensor *accel_z_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *gyro_x_sensor_{nullptr};
  sensor::Sensor *gyro_y_sensor_{nullptr};
  sensor::Sensor *gyro_z_sensor_{nullptr};

  uint16_t temp_sensitivity_ = 16;
  bool high_perf_ = false;
  uint16_t sample_gyro_rate_ = 0;
  uint16_t sample_accl_rate_ = 0;
  bool _power_save = false;
  bool do_sleep_ = false;
  bool has_accl_temp_ = false;
  bool _has_gyro = false;
  bool is_sleeping_ = false;
  uint8_t accl_conf_ = 0;
  uint8_t gyro_conf_ = 0;

  void _read_sensor(uint8_t reg, sensor::Sensor *sensor, SensorType type, bool do_publish);
};
;

}  // namespace LSM6DS3
}  // namespace esphome
