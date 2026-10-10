#pragma once

#include "esphome/components/motion/motion_component.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/i2c/i2c.h"
#include "bmm150_aux.h"
#include <functional>

namespace esphome::bmi270 {

//  Register map
static constexpr uint8_t BMI270_REG_CHIP_ID = 0x00;
static constexpr uint8_t BMI270_REG_ERR_REG = 0x02;
static constexpr uint8_t BMI270_REG_STATUS = 0x03;
static constexpr uint8_t BMI270_REG_DATA_8 = 0x0C;   // ACC_X LSB
static constexpr uint8_t BMI270_REG_DATA_14 = 0x12;  // GYR_X LSB
static constexpr uint8_t BMI270_REG_TEMP_0 = 0x22;
static constexpr uint8_t BMI270_REG_TEMP_MSB = 0x23;  // temperature (2 bytes big-endian ish)

static constexpr uint8_t REG_READ_LEN =
    BMI270_REG_TEMP_MSB - BMI270_REG_DATA_8 +
    1;  // 0x23 - 0x0C + 1 = 0x18 bytes total for accel(6) + gyro(6) + temp(2) + padding(4)

static constexpr uint8_t BMI270_REG_PWR_CONF = 0x7C;
static constexpr uint8_t BMI270_REG_PWR_CTRL = 0x7D;
static constexpr uint8_t BMI270_REG_INIT_CTRL = 0x59;
static constexpr uint8_t BMI270_REG_INIT_DATA = 0x5E;
static constexpr uint8_t BMI270_REG_INIT_ADDR_0 = 0x5B;
static constexpr uint8_t BMI270_REG_INTERNAL_STATUS = 0x21;
static constexpr uint8_t BMI270_REG_ACC_CONF = 0x40;
static constexpr uint8_t BMI270_REG_ACC_RANGE = 0x41;
static constexpr uint8_t BMI270_REG_GYR_CONF = 0x42;
static constexpr uint8_t BMI270_REG_GYR_RANGE = 0x43;

static constexpr uint8_t BMI270_CHIP_ID_VALUE = 0x24;

//  Auxiliary (secondary I2C master) interface registers.
//  Used to optionally drive a BMM150 magnetometer wired to the BMI270's AUX pins
//  instead of the main I2C bus - the BMM150 has no address of its own on that bus.
static constexpr uint8_t BMI270_REG_AUX_X_LSB = 0x04;  // start of aux data: X,Y,Z,R (8 bytes)
static constexpr uint8_t BMI270_REG_AUX_DEV_ID = 0x4B;
static constexpr uint8_t BMI270_REG_AUX_IF_CONF = 0x4C;
static constexpr uint8_t BMI270_REG_AUX_RD_ADDR = 0x4D;
static constexpr uint8_t BMI270_REG_AUX_WR_ADDR = 0x4E;
static constexpr uint8_t BMI270_REG_AUX_WR_DATA = 0x4F;
static constexpr uint8_t BMI270_REG_IF_CONF = 0x6B;
static constexpr uint8_t BMI270_REG_CMD = 0x7E;

// Register values and bitfields
static constexpr uint8_t BMI270_CMD_SOFT_RESET = 0xB6;
static constexpr uint8_t BMI270_PWR_CONF_ADV_POWER_SAVE_OFF = 0x00;
static constexpr uint8_t BMI270_PWR_CONF_FIFO_SELF_WAKEUP = 0x02;
static constexpr uint8_t BMI270_INIT_CTRL_LOAD_START = 0x00;
static constexpr uint8_t BMI270_INIT_CTRL_LOAD_DONE = 0x01;
static constexpr uint8_t BMI270_INTERNAL_STATUS_MASK = 0x0F;
static constexpr uint8_t BMI270_INTERNAL_STATUS_INIT_OK = 0x01;
static constexpr uint8_t BMI270_CONF_BWP_NORMAL = 0x02;  // filter bandwidth, bits 5:4
static constexpr uint8_t BMI270_CONF_BWP_SHIFT = 4;
static constexpr uint8_t BMI270_ACC_CONF_PERF_MODE = 0x80;
static constexpr uint8_t BMI270_GYR_CONF_NOISE_PERF = 0x40;
static constexpr uint8_t BMI270_GYR_CONF_FILTER_PERF = 0x80;
static constexpr uint8_t BMI270_STATUS_AUX_BUSY = 0x04;
static constexpr uint8_t BMI270_IF_CONF_AUX_IF_EN = 0x20;
static constexpr uint8_t BMI270_PWR_CTRL_AUX_EN = 0x01;
static constexpr uint8_t BMI270_PWR_CTRL_ACC_EN = 0x02;
static constexpr uint8_t BMI270_PWR_CTRL_GYR_EN = 0x04;
static constexpr uint8_t BMI270_PWR_CTRL_TEMP_EN = 0x08;
static constexpr uint8_t BMI270_PWR_CTRL_IMU =
    BMI270_PWR_CTRL_ACC_EN | BMI270_PWR_CTRL_GYR_EN | BMI270_PWR_CTRL_TEMP_EN;
static constexpr uint8_t BMI270_AUX_IF_CONF_MANUAL_EN = 0x80;
static constexpr uint8_t BMI270_AUX_IF_CONF_FCU_WRITE_EN = 0x40;
static constexpr uint8_t BMI270_AUX_IF_CONF_MAN_RD_BURST_8 = 0x0C;  // bits 5:2 = 3
static constexpr uint8_t BMI270_AUX_IF_CONF_RD_BURST_8 = 0x03;      // bits 1:0 = 3

// AUX_X_LSB..AUX_R_MSB (8 bytes) sit directly before the accel data, so one burst can read both.
static constexpr uint8_t BMI270_AUX_DATA_LEN = 8;
static_assert(BMI270_REG_AUX_X_LSB + BMI270_AUX_DATA_LEN == BMI270_REG_DATA_8, "aux data must precede accel data");

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

// What, if anything, is wired to the BMI270's auxiliary (secondary I2C master) interface.
// Only one aux device can be attached at a time - it's a single secondary bus, not a mux.
enum BMI270AuxDevice : uint8_t {
  BMI270_AUX_DEVICE_NONE = 0,
  BMI270_AUX_DEVICE_BMM150 = 1,
};

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
  void set_aux_device(BMI270AuxDevice device) { this->aux_device_ = device; }
  void set_aux_device_address(uint8_t address) { this->aux_device_address_ = address; }
  template<typename F> void add_temperature_listener(F &&cb) { this->temperature_callback_.add(std::forward<F>(cb)); }
  template<typename F> void add_magnetometer_listener(F &&cb) { this->magnetometer_callback_.add(std::forward<F>(cb)); }

 protected:
  bool update_data(motion::MotionData &data) override;
  bool load_config_file_();
  // Brings up the BMM150 over the BMI270's aux interface. Optional: on failure this only
  // disables magnetometer reporting, it does not fail the whole component (accel/gyro still work).
  bool setup_magnetometer_();
  // Blocking poll of the aux-transaction-busy bit; aux transaction time depends on the
  // attached device, so this is bounded by retries/timeout rather than a fixed delay.
  bool wait_for_aux_idle_(uint32_t timeout_ms);
  // Manual-mode aux read of 8 bytes starting at `reg` on the aux device.
  bool read_aux_block_(uint8_t reg, uint8_t *buf);
  // Manual-mode aux write of one byte to `reg` on the aux device.
  bool write_aux_register_(uint8_t reg, uint8_t value);
  // Reads the BMM150 factory trim values used to compensate its samples.
  bool read_magnetometer_trim_();

  // Config
  BMI270AccelRange accel_range_{BMI270_ACCEL_RANGE_4G};
  BMI270AccelODR accel_odr_{BMI270_ACCEL_ODR_100};
  BMI270GyroRange gyro_range_{BMI270_GYRO_RANGE_2000};
  BMI270GyroODR gyro_odr_{BMI270_GYRO_ODR_200};
  BMI270AuxDevice aux_device_{BMI270_AUX_DEVICE_NONE};
  uint8_t aux_device_address_{bmm150::BMM150_DEFAULT_I2C_ADDRESS};
  bool magnetometer_ready_{false};
  bmm150::BMM150Trim mag_trim_{};

  LazyCallbackManager<void(float)> temperature_callback_{};
  LazyCallbackManager<void(bmm150::BMM150Data &)> magnetometer_callback_{};
};

}  // namespace esphome::bmi270
