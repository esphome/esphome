#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace bmi270 {

// ── Register map ────────────────────────────────────────────────────────────
static const uint8_t BMI270_REG_CHIP_ID = 0x00;
static const uint8_t BMI270_REG_ERR_REG = 0x02;
static const uint8_t BMI270_REG_STATUS = 0x03;
static const uint8_t BMI270_REG_DATA_8 = 0x0C;    // ACC_X LSB
static const uint8_t BMI270_REG_DATA_14 = 0x12;   // GYR_X LSB
static const uint8_t BMI270_REG_TEMP_MSB = 0x23;  // temperature (2 bytes big-endian ish)
static const uint8_t BMI270_REG_TEMP_0 = 0x22;
static const uint8_t BMI270_REG_PWR_CONF = 0x7C;
static const uint8_t BMI270_REG_PWR_CTRL = 0x7D;
static const uint8_t BMI270_REG_INIT_CTRL = 0x59;
static const uint8_t BMI270_REG_INIT_DATA = 0x5E;
static const uint8_t BMI270_REG_INIT_ADDR_0 = 0x5B;
static const uint8_t BMI270_REG_INTERNAL_STATUS = 0x21;
static const uint8_t BMI270_REG_ACC_CONF = 0x40;
static const uint8_t BMI270_REG_ACC_RANGE = 0x41;
static const uint8_t BMI270_REG_GYR_CONF = 0x42;
static const uint8_t BMI270_REG_GYR_RANGE = 0x43;

static const uint8_t BMI270_CHIP_ID_VALUE = 0x24;

// ── Accelerometer range options ──────────────────────────────────────────────
enum BMI270AccelRange : uint8_t {
  BMI270_ACCEL_RANGE_2G = 0x00,
  BMI270_ACCEL_RANGE_4G = 0x01,
  BMI270_ACCEL_RANGE_8G = 0x02,
  BMI270_ACCEL_RANGE_16G = 0x03,
};

// ── Accelerometer ODR options ────────────────────────────────────────────────
enum BMI270AccelODR : uint8_t {
  BMI270_ACCEL_ODR_12_5 = 0x05,
  BMI270_ACCEL_ODR_25 = 0x06,
  BMI270_ACCEL_ODR_50 = 0x07,
  BMI270_ACCEL_ODR_100 = 0x08,
  BMI270_ACCEL_ODR_200 = 0x09,
  BMI270_ACCEL_ODR_400 = 0x0A,
  BMI270_ACCEL_ODR_800 = 0x0B,
  BMI270_ACCEL_ODR_1600 = 0x0C,
};

// ── Gyroscope range options ──────────────────────────────────────────────────
enum BMI270GyroRange : uint8_t {
  BMI270_GYRO_RANGE_2000 = 0x00,
  BMI270_GYRO_RANGE_1000 = 0x01,
  BMI270_GYRO_RANGE_500 = 0x02,
  BMI270_GYRO_RANGE_250 = 0x03,
  BMI270_GYRO_RANGE_125 = 0x04,
};

// ── Gyroscope ODR options ────────────────────────────────────────────────────
enum BMI270GyroODR : uint8_t {
  BMI270_GYRO_ODR_25 = 0x06,
  BMI270_GYRO_ODR_50 = 0x07,
  BMI270_GYRO_ODR_100 = 0x08,
  BMI270_GYRO_ODR_200 = 0x09,
  BMI270_GYRO_ODR_400 = 0x0A,
  BMI270_GYRO_ODR_800 = 0x0B,
  BMI270_GYRO_ODR_1600 = 0x0C,
  BMI270_GYRO_ODR_3200 = 0x0D,
};

// ── Main component class ─────────────────────────────────────────────────────
class BMI270Component : public PollingComponent, public i2c::I2CDevice {
 public:
  // Lifecycle
  void setup() override;
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Sensor setters (called from Python codegen)
  void set_accel_x_sensor(sensor::Sensor *s) { accel_x_ = s; }
  void set_accel_y_sensor(sensor::Sensor *s) { accel_y_ = s; }
  void set_accel_z_sensor(sensor::Sensor *s) { accel_z_ = s; }
  void set_gyro_x_sensor(sensor::Sensor *s) { gyro_x_ = s; }
  void set_gyro_y_sensor(sensor::Sensor *s) { gyro_y_ = s; }
  void set_gyro_z_sensor(sensor::Sensor *s) { gyro_z_ = s; }
  void set_temperature_sensor(sensor::Sensor *s) { temperature_ = s; }

  // Configuration setters
  void set_accel_range(BMI270AccelRange r) { accel_range_ = r; }
  void set_accel_odr(BMI270AccelODR o) { accel_odr_ = o; }
  void set_gyro_range(BMI270GyroRange r) { gyro_range_ = r; }
  void set_gyro_odr(BMI270GyroODR o) { gyro_odr_ = o; }

 protected:
  bool load_config_file_();
  bool write_reg_(uint8_t reg, uint8_t value);
  bool read_reg_(uint8_t reg, uint8_t *value);
  bool read_bytes_(uint8_t reg, uint8_t *data, size_t len);

  // Sensors
  sensor::Sensor *accel_x_{nullptr};
  sensor::Sensor *accel_y_{nullptr};
  sensor::Sensor *accel_z_{nullptr};
  sensor::Sensor *gyro_x_{nullptr};
  sensor::Sensor *gyro_y_{nullptr};
  sensor::Sensor *gyro_z_{nullptr};
  sensor::Sensor *temperature_{nullptr};

  // Config
  BMI270AccelRange accel_range_{BMI270_ACCEL_RANGE_4G};
  BMI270AccelODR accel_odr_{BMI270_ACCEL_ODR_100};
  BMI270GyroRange gyro_range_{BMI270_GYRO_RANGE_2000};
  BMI270GyroODR gyro_odr_{BMI270_GYRO_ODR_200};

  bool init_ok_{false};
};

}  // namespace bmi270
}  // namespace esphome
