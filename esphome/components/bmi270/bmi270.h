#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/i2c/i2c.h"
#include <array>
#include <functional>

namespace esphome {
namespace bmi270 {

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
    1;  // 0x23 - 0x0C + 1 = 18 bytes total for accel(6) + gyro(6) + temp(2) + padding(4)

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

class BMI270AccelData {
 public:
  float acceleration[3]{NAN, NAN, NAN};
  float angular_rate[3]{NAN, NAN, NAN};
  float temperature{NAN};
};

// Main component class
class BMI270Component : public PollingComponent, public i2c::I2CDevice {
 public:
  // Lifecycle
  void setup() override;
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Configuration setters
  void set_accel_range(BMI270AccelRange r) { accel_range_ = r; }
  void set_accel_odr(BMI270AccelODR o) { accel_odr_ = o; }
  void set_gyro_range(BMI270GyroRange r) { gyro_range_ = r; }
  void set_gyro_odr(BMI270GyroODR o) { gyro_odr_ = o; }
  void set_matrix(const std::array<float, 9> &m) { memcpy(this->matrix_, m.data(), sizeof(this->matrix_)); }
  void add_listener(std::function<void(BMI270AccelData &)> &&cb) { accel_data_callback_.add(std::move(cb)); }

 protected:
  bool load_config_file_();

  // Config
  BMI270AccelRange accel_range_{BMI270_ACCEL_RANGE_4G};
  BMI270AccelODR accel_odr_{BMI270_ACCEL_ODR_100};
  BMI270GyroRange gyro_range_{BMI270_GYRO_RANGE_2000};
  BMI270GyroODR gyro_odr_{BMI270_GYRO_ODR_200};
  float matrix_[9]{
      1, 0, 0, 0, 1, 0, 0, 0, 1,
  };

  void map_axes_(float output[3], float x, float y, float z) const {
    output[0] = x * this->matrix_[0] + y * this->matrix_[1] + z * this->matrix_[2];
    output[1] = x * this->matrix_[3] + y * this->matrix_[4] + z * this->matrix_[5];
    output[2] = x * this->matrix_[6] + y * this->matrix_[7] + z * this->matrix_[8];
  };

  LazyCallbackManager<void(BMI270AccelData &)> accel_data_callback_{};
};

}  // namespace bmi270
}  // namespace esphome
