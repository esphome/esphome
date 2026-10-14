#pragma once

#include "esphome/components/motion/motion_component.h"
#include "esphome/components/spi/spi.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome::icm20689 {

// Register map
static constexpr uint8_t ICM20689_REG_WHO_AM_I = 0x75;
static constexpr uint8_t ICM20689_REG_PWR_MGMT_1 = 0x6B;
static constexpr uint8_t ICM20689_REG_USER_CTRL = 0x6A;
static constexpr uint8_t ICM20689_REG_GYRO_CONFIG = 0x1B;
static constexpr uint8_t ICM20689_REG_ACCEL_CONFIG = 0x1C;
static constexpr uint8_t ICM20689_REG_ACCEL_XOUT_H = 0x3B;
static constexpr uint8_t ICM20689_READ_BIT = 0x80;
static constexpr uint8_t ICM20689_WHO_AM_I_VALUE = 0x98;
static constexpr uint8_t ICM20689_SOFT_RESET = 0x81;   // PWR_MGMT_1: DEVICE_RESET | CLKSEL=1 (datasheet 4.20)
static constexpr uint8_t ICM20689_CLKSEL_AUTO = 0x01;  // PWR_MGMT_1: SLEEP=0, CLKSEL=1 (auto-select best clock)
static constexpr uint8_t ICM20689_I2C_IF_DIS = 0x10;   // USER_CTRL bit 4

// Accelerometer range options: FS_SEL bits [4:3] of ACCEL_CONFIG
enum ICM20689AccelRange : uint8_t {
  ICM20689_ACCEL_RANGE_2G = 0x00,
  ICM20689_ACCEL_RANGE_4G = 0x01,
  ICM20689_ACCEL_RANGE_8G = 0x02,
  ICM20689_ACCEL_RANGE_16G = 0x03,
};

// Gyroscope range options: FS_SEL bits [4:3] of GYRO_CONFIG
enum ICM20689GyroRange : uint8_t {
  ICM20689_GYRO_RANGE_250 = 0x00,
  ICM20689_GYRO_RANGE_500 = 0x01,
  ICM20689_GYRO_RANGE_1000 = 0x02,
  ICM20689_GYRO_RANGE_2000 = 0x03,
};

class ICM20689Component final : public motion::MotionComponent,
                                public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                                      spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_1MHZ> {
 public:
  void setup() override;
  void dump_config() override;

  void set_accel_range(ICM20689AccelRange range) { this->accel_range_ = range; }
  void set_gyro_range(ICM20689GyroRange range) { this->gyro_range_ = range; }

  template<typename F> void add_temperature_listener(F &&callback) {
    this->temperature_callback_.add(std::forward<F>(callback));
  }

 protected:
  bool update_data(motion::MotionData &data) override;

  void write_register_(uint8_t reg, uint8_t value);
  // Reads len bytes starting at reg in one CS-low transaction.
  void read_registers_(uint8_t reg, uint8_t *data, size_t len);

  ICM20689AccelRange accel_range_{ICM20689_ACCEL_RANGE_2G};
  ICM20689GyroRange gyro_range_{ICM20689_GYRO_RANGE_250};

  LazyCallbackManager<void(float)> temperature_callback_{};
};

}  // namespace esphome::icm20689
