#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/motion/motion_component.h"

namespace esphome::lsm6ds {

// ── Register map (datasheet DocID030071 Rev 3, Table 19) ────────────────────
static const uint8_t LSM6DS_REG_WHO_AM_I = 0x0F;
static const uint8_t LSM6DS_REG_CTRL1_XL = 0x10;    // Accel ODR + FS
static const uint8_t LSM6DS_REG_CTRL2_G = 0x11;     // Gyro ODR + FS
static const uint8_t LSM6DS_REG_CTRL3_C = 0x12;     // SW_RESET, BDU, IF_INC
static const uint8_t LSM6DS_REG_CTRL6_C = 0x15;     // Accel HP disable, Gyro LPF1
static const uint8_t LSM6DS_REG_CTRL7_G = 0x16;     // Gyro HP disable
static const uint8_t LSM6DS_REG_STATUS = 0x1E;      // XLDA, GDA, TDA
static const uint8_t LSM6DS_REG_OUT_TEMP_L = 0x20;  // Temperature LSB
static const uint8_t LSM6DS_REG_OUTX_L_G = 0x22;    // Gyro X LSB  (burst start)
static const uint8_t LSM6DS_REG_OUTX_L_XL = 0x28;   // Accel X LSB

// Burst read from 0x22 to 0x2D inclusive: gyro XYZ (6 bytes) + accel XYZ (6 bytes)
static const uint8_t LSM6DS_BURST_LEN = 12;
static const uint8_t LSM6DS_ACCEL_OFFSET = 6;  // 0x28 - 0x22

// ── CTRL3_C bit fields ───────────────────────────────────────────────────────
static const uint8_t CTRL3_C_SW_RESET = (1 << 0);
static const uint8_t CTRL3_C_IF_INC = (1 << 2);  // auto-increment address on burst (default 1)
static const uint8_t CTRL3_C_BDU = (1 << 6);     // block data update

// ── Accelerometer full-scale range ──────────────────────────────────────────
// CTRL1_XL bits [3:2] — FS_XL[1:0]
// Note: 0x01 = ±16g is intentional per Table 52 — the mapping is non-monotonic
enum LSM6DSAccelRange : uint8_t {
  LSM6DS_ACCEL_RANGE_2G = 0x00,   // ±2 g,  0.061 mg/LSB
  LSM6DS_ACCEL_RANGE_16G = 0x01,  // ±16 g, 0.488 mg/LSB
  LSM6DS_ACCEL_RANGE_4G = 0x02,   // ±4 g,  0.122 mg/LSB
  LSM6DS_ACCEL_RANGE_8G = 0x03,   // ±8 g,  0.244 mg/LSB
};

// ── Accelerometer output data rate ──────────────────────────────────────────
// CTRL1_XL bits [7:4] — ODR_XL[3:0]
enum LSM6DSAccelODR : uint8_t {
  LSM6DS_ACCEL_ODR_OFF = 0x00,
  LSM6DS_ACCEL_ODR_12_5 = 0x01,  // 12.5 Hz
  LSM6DS_ACCEL_ODR_26 = 0x02,    // 26 Hz
  LSM6DS_ACCEL_ODR_52 = 0x03,    // 52 Hz
  LSM6DS_ACCEL_ODR_104 = 0x04,   // 104 Hz
  LSM6DS_ACCEL_ODR_208 = 0x05,   // 208 Hz
  LSM6DS_ACCEL_ODR_416 = 0x06,   // 416 Hz
  LSM6DS_ACCEL_ODR_833 = 0x07,   // 833 Hz
  LSM6DS_ACCEL_ODR_1666 = 0x08,  // 1666 Hz
  LSM6DS_ACCEL_ODR_3332 = 0x09,  // 3332 Hz
  LSM6DS_ACCEL_ODR_6664 = 0x0A,  // 6664 Hz
};

// ── Gyroscope full-scale range ───────────────────────────────────────────────
// CTRL2_G bits [3:0] — FS_G[2:1] and FS_125 (bit 1)
// The FS_125 bit (bit 1) enables the ±125 dps range independently of FS_G.
// For all other ranges, bits [3:2] select the range and bit 1 = 0.
enum LSM6DSGyroRange : uint8_t {
  LSM6DS_GYRO_RANGE_125 = 0x02,   // ±125  dps, 4.375 mdps/LSB (FS_125=1)
  LSM6DS_GYRO_RANGE_250 = 0x00,   // ±250  dps, 8.75  mdps/LSB
  LSM6DS_GYRO_RANGE_500 = 0x04,   // ±500  dps, 17.50 mdps/LSB
  LSM6DS_GYRO_RANGE_1000 = 0x08,  // ±1000 dps, 35    mdps/LSB
  LSM6DS_GYRO_RANGE_2000 = 0x0C,  // ±2000 dps, 70    mdps/LSB
};

// ── Gyroscope output data rate ───────────────────────────────────────────────
// CTRL2_G bits [7:4] — ODR_G[3:0]
enum LSM6DSGyroODR : uint8_t {
  LSM6DS_GYRO_ODR_OFF = 0x00,
  LSM6DS_GYRO_ODR_12_5 = 0x01,  // 12.5 Hz
  LSM6DS_GYRO_ODR_26 = 0x02,    // 26 Hz
  LSM6DS_GYRO_ODR_52 = 0x03,    // 52 Hz
  LSM6DS_GYRO_ODR_104 = 0x04,   // 104 Hz
  LSM6DS_GYRO_ODR_208 = 0x05,   // 208 Hz
  LSM6DS_GYRO_ODR_416 = 0x06,   // 416 Hz
  LSM6DS_GYRO_ODR_833 = 0x07,   // 833 Hz
  LSM6DS_GYRO_ODR_1666 = 0x08,  // 1666 Hz
  LSM6DS_GYRO_ODR_3332 = 0x09,  // 3332 Hz
  LSM6DS_GYRO_ODR_6664 = 0x0A,  // 6664 Hz
};

// ── Main component class ─────────────────────────────────────────────────────
class LSM6DSComponent : public motion::MotionComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Configuration setters (called from Python codegen)
  void set_accel_range(LSM6DSAccelRange r) { this->accel_range_ = r; }
  void set_accel_odr(LSM6DSAccelODR o) { this->accel_odr_ = o; }
  void set_gyro_range(LSM6DSGyroRange r) { this->gyro_range_ = r; }
  void set_gyro_odr(LSM6DSGyroODR o) { this->gyro_odr_ = o; }

  template<typename F> void add_temperature_listener(F &&cb) { this->temperature_callback_.add(std::forward<F>(cb)); }

 protected:
  const char *chip_name_{"Unknown"};
  bool update_data(motion::MotionData &data) override;

  LSM6DSAccelRange accel_range_{LSM6DS_ACCEL_RANGE_4G};
  LSM6DSAccelODR accel_odr_{LSM6DS_ACCEL_ODR_104};
  LSM6DSGyroRange gyro_range_{LSM6DS_GYRO_RANGE_2000};
  LSM6DSGyroODR gyro_odr_{LSM6DS_GYRO_ODR_208};

  LazyCallbackManager<void(float)> temperature_callback_{};
};

}  // namespace esphome::lsm6ds
