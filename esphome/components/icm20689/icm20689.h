#pragma once

#include "esphome/components/motion/motion_component.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/spi/spi.h"
#include <functional>

namespace esphome::icm20689 {

//  Register map (public datasheet, fixed-function silicon)
static const uint8_t ICM20689_REG_WHO_AM_I = 0x75;
static const uint8_t ICM20689_REG_PWR_MGMT_1 = 0x6B;
static const uint8_t ICM20689_REG_GYRO_CONFIG = 0x1B;
static const uint8_t ICM20689_REG_ACCEL_CONFIG = 0x1C;
static const uint8_t ICM20689_REG_ACCEL_XOUT_H = 0x3B;  // 14 bytes: accel(6) + temp(2) + gyro(6)
static const uint8_t ICM20689_REG_TEMP_OUT_H = 0x41;
static const uint8_t ICM20689_READ_BIT = 0x80;
static const uint8_t ICM20689_WHO_AM_I_VALUE = 0x98;

//  Accelerometer range options -- FS_SEL bits [4:3] of ACCEL_CONFIG
enum ICM20689AccelRange : uint8_t {
  ICM20689_ACCEL_RANGE_2G = 0x00,
  ICM20689_ACCEL_RANGE_4G = 0x01,
  ICM20689_ACCEL_RANGE_8G = 0x02,
  ICM20689_ACCEL_RANGE_16G = 0x03,
};

//  Gyroscope range options -- FS_SEL bits [4:3] of GYRO_CONFIG
enum ICM20689GyroRange : uint8_t {
  ICM20689_GYRO_RANGE_250 = 0x00,
  ICM20689_GYRO_RANGE_500 = 0x01,
  ICM20689_GYRO_RANGE_1000 = 0x02,
  ICM20689_GYRO_RANGE_2000 = 0x03,
};

// Main component class
class ICM20689Component final : public motion::MotionComponent,
                                public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                                      spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_1MHZ> {
 public:
  // Lifecycle
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Configuration setters
  void set_accel_range(ICM20689AccelRange r) { this->accel_range_ = r; }
  void set_gyro_range(ICM20689GyroRange r) { this->gyro_range_ = r; }
  template<typename F> void add_temperature_listener(F &&cb) { this->temperature_callback_.add(std::forward<F>(cb)); }

 protected:
  bool update_data(motion::MotionData &data) override;

  void write_register_(uint8_t reg, uint8_t value);
  // Reads len bytes starting at reg into data, in one CS-low transaction.
  void read_registers_(uint8_t reg, uint8_t *data, size_t len);

  // Config
  ICM20689AccelRange accel_range_{ICM20689_ACCEL_RANGE_2G};
  ICM20689GyroRange gyro_range_{ICM20689_GYRO_RANGE_250};

  LazyCallbackManager<void(float)> temperature_callback_{};
};

}  // namespace esphome::icm20689
