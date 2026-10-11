#pragma once

#include "esphome/components/motion/motion_component.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/i2c/i2c.h"
#include <functional>

namespace esphome::bmi270 {

//  Register map
static const uint8_t BMI270_REG_CHIP_ID = 0x00;
static const uint8_t BMI270_REG_ERR_REG = 0x02;
static const uint8_t BMI270_REG_STATUS = 0x03;
static const uint8_t BMI270_REG_DATA_8 = 0x0C;   // ACC_X LSB
static const uint8_t BMI270_REG_DATA_14 = 0x12;  // GYR_X LSB
static const uint8_t BMI270_REG_TEMP_0 = 0x22;
static const uint8_t BMI270_REG_TEMP_MSB = 0x23;  // temperature (2 bytes big-endian ish)

static constexpr uint8_t REG_READ_LEN =
    BMI270_REG_TEMP_MSB - BMI270_REG_DATA_8 +
    1;  // 0x23 - 0x0C + 1 = 0x18 bytes total for accel(6) + gyro(6) + temp(2) + padding(4)

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

//  Accelerometer range options
enum BMI270AccelRange : uint8_t {
  BMI270_ACCEL_RANGE_2G = 0x00,
  BMI270_ACCEL_RANGE_4G = 0x01,
  BMI270_ACCEL_RANGE_8G = 0x02,
  BMI270_ACCEL_RANGE_16G = 0x03,
};

//  Accelerometer ODR options
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

// Gyroscope range options
enum BMI270GyroRange : uint8_t {
  BMI270_GYRO_RANGE_2000 = 0x00,
  BMI270_GYRO_RANGE_1000 = 0x01,
  BMI270_GYRO_RANGE_500 = 0x02,
  BMI270_GYRO_RANGE_250 = 0x03,
  BMI270_GYRO_RANGE_125 = 0x04,
};

// Gyroscope ODR options
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

// ---Data class

// Main component class
class BMI270Component final : public motion::MotionComponent, public i2c::I2CDevice {
 public:
  // Lifecycle
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Configuration setters
  void set_accel_range(BMI270AccelRange r) { this->accel_range_ = r; }
  void set_accel_odr(BMI270AccelODR o) { this->accel_odr_ = o; }
  void set_gyro_range(BMI270GyroRange r) { this->gyro_range_ = r; }
  void set_gyro_odr(BMI270GyroODR o) { this->gyro_odr_ = o; }
  template<typename F> void add_temperature_listener(F &&cb) { this->temperature_callback_.add(std::forward<F>(cb)); }

 protected:
  bool update_data(motion::MotionData &data) override;
  bool load_config_file_();

  // Config
  BMI270AccelRange accel_range_{BMI270_ACCEL_RANGE_4G};
  BMI270AccelODR accel_odr_{BMI270_ACCEL_ODR_100};
  BMI270GyroRange gyro_range_{BMI270_GYRO_RANGE_2000};
  BMI270GyroODR gyro_odr_{BMI270_GYRO_ODR_200};

  LazyCallbackManager<void(float)> temperature_callback_{};
};

}  // namespace esphome::bmi270
