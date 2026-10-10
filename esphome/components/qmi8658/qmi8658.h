#pragma once

#include "esphome/components/motion/motion_component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome::qmi8658 {

//  Register map
static constexpr uint8_t QMI8658_REG_WHO_AM_I = 0x00;
static constexpr uint8_t QMI8658_REG_REVISION = 0x01;
static constexpr uint8_t QMI8658_REG_CTRL1 = 0x02;  // serial interface / auto-increment
static constexpr uint8_t QMI8658_REG_CTRL2 = 0x03;  // accelerometer ODR / range
static constexpr uint8_t QMI8658_REG_CTRL3 = 0x04;  // gyroscope ODR / range
static constexpr uint8_t QMI8658_REG_CTRL5 = 0x06;  // low-pass filter
static constexpr uint8_t QMI8658_REG_CTRL7 = 0x08;  // sensor enable
static constexpr uint8_t QMI8658_REG_STATUS0 = 0x2E;
static constexpr uint8_t QMI8658_REG_TEMP_BASE = 0x33;  // start of the data block
static constexpr uint8_t QMI8658_REG_TEMP_L = 0x33;     // Low byte of temperature
static constexpr uint8_t QMI8658_REG_AX_L = 0x35;
static constexpr uint8_t QMI8658_REG_GX_L = 0x3B;
static constexpr uint8_t QMI8658_REG_RESET = 0x60;

// One contiguous read covers temperature (2) + accel (6) + gyro (6) starting at TEMP_L.
static constexpr uint8_t REG_READ_LEN = QMI8658_REG_GX_L + 6 - QMI8658_REG_TEMP_BASE;  // 0x41 - 0x33 = 14
static constexpr uint8_t TEMP_OFFS = QMI8658_REG_TEMP_L - QMI8658_REG_TEMP_BASE;       // 0
static constexpr uint8_t ACC_OFFS = QMI8658_REG_AX_L - QMI8658_REG_TEMP_BASE;          // 2
static constexpr uint8_t GYR_OFFS = QMI8658_REG_GX_L - QMI8658_REG_TEMP_BASE;          // 8

static constexpr uint8_t QMI8658_WHO_AM_I_VALUE = 0x05;
static constexpr uint8_t QMI8658_RESET_CMD = 0xB0;
// CTRL1: bit6 ADDR_AI (register address auto-increment); little-endian, 4-wire SPI
static constexpr uint8_t QMI8658_CTRL1_VALUE = 0x40;
// CTRL7: aEN (bit0) | gEN (bit1)
static constexpr uint8_t QMI8658_CTRL7_ACC_EN = 0x01;
static constexpr uint8_t QMI8658_CTRL7_GYR_EN = 0x02;

// Accelerometer range options (CTRL2 bits 6:4)
enum QMI8658AccelRange : uint8_t {
  QMI8658_ACCEL_RANGE_2G = 0x00,
  QMI8658_ACCEL_RANGE_4G = 0x10,
  QMI8658_ACCEL_RANGE_8G = 0x20,
  QMI8658_ACCEL_RANGE_16G = 0x30,
};

// Accelerometer ODR options (CTRL2 bits 3:0)
enum QMI8658AccelODR : uint8_t {
  QMI8658_ACCEL_ODR_8000 = 0x00,
  QMI8658_ACCEL_ODR_4000 = 0x01,
  QMI8658_ACCEL_ODR_2000 = 0x02,
  QMI8658_ACCEL_ODR_1000 = 0x03,
  QMI8658_ACCEL_ODR_500 = 0x04,
  QMI8658_ACCEL_ODR_250 = 0x05,
  QMI8658_ACCEL_ODR_125 = 0x06,
  QMI8658_ACCEL_ODR_62_5 = 0x07,
  QMI8658_ACCEL_ODR_31_25 = 0x08,
};

// Gyroscope range options (CTRL3 bits 6:4)
enum QMI8658GyroRange : uint8_t {
  QMI8658_GYRO_RANGE_16 = 0x00,
  QMI8658_GYRO_RANGE_32 = 0x10,
  QMI8658_GYRO_RANGE_64 = 0x20,
  QMI8658_GYRO_RANGE_128 = 0x30,
  QMI8658_GYRO_RANGE_256 = 0x40,
  QMI8658_GYRO_RANGE_512 = 0x50,
  QMI8658_GYRO_RANGE_1024 = 0x60,
  QMI8658_GYRO_RANGE_2048 = 0x70,
};

// Gyroscope ODR options (CTRL3 bits 3:0)
enum QMI8658GyroODR : uint8_t {
  QMI8658_GYRO_ODR_8000 = 0x00,
  QMI8658_GYRO_ODR_4000 = 0x01,
  QMI8658_GYRO_ODR_2000 = 0x02,
  QMI8658_GYRO_ODR_1000 = 0x03,
  QMI8658_GYRO_ODR_500 = 0x04,
  QMI8658_GYRO_ODR_250 = 0x05,
  QMI8658_GYRO_ODR_125 = 0x06,
  QMI8658_GYRO_ODR_62_5 = 0x07,
  QMI8658_GYRO_ODR_31_25 = 0x08,
};

// Main component class
class QMI8658Component : public motion::MotionComponent, public i2c::I2CDevice {
 public:
  // Lifecycle
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Configuration setters
  void set_accel_range(QMI8658AccelRange r) { this->accel_range_ = r; }
  void set_accel_odr(QMI8658AccelODR o) { this->accel_odr_ = o; }
  void set_gyro_range(QMI8658GyroRange r) { this->gyro_range_ = r; }
  void set_gyro_odr(QMI8658GyroODR o) { this->gyro_odr_ = o; }
  template<typename F> void add_temperature_listener(F &&cb) { this->temperature_callback_.add(std::forward<F>(cb)); }

 protected:
  bool update_data(motion::MotionData &data) override;

  // Config
  QMI8658AccelRange accel_range_{QMI8658_ACCEL_RANGE_4G};
  QMI8658AccelODR accel_odr_{QMI8658_ACCEL_ODR_1000};
  QMI8658GyroRange gyro_range_{QMI8658_GYRO_RANGE_2048};
  QMI8658GyroODR gyro_odr_{QMI8658_GYRO_ODR_1000};

  LazyCallbackManager<void(float)> temperature_callback_{};
};

}  // namespace esphome::qmi8658
