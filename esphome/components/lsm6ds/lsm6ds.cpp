#include "lsm6ds.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome::lsm6ds {

static const char *const TAG = "lsm6ds";

static const struct {
  uint8_t who_am_i;
  const char *const name;
} CHIP_IDS[] = {{0x69, "LSMDSO"}, {0x6A, "LSM6DS3"}};

void LSM6DSComponent::setup() {
  MotionComponent::setup();
  uint8_t who_am_i = 0;
  if (this->read_register(LSM6DS_REG_WHO_AM_I, &who_am_i, 1) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Failed to read WHO_AM_I — check wiring and I2C address");
    this->mark_failed();
    return;
  }
  const char *chip_name = nullptr;
  for (const auto &chip : CHIP_IDS) {
    if (chip.who_am_i == who_am_i) {
      chip_name = chip.name;
      break;
    }
  }
  if (chip_name == nullptr) {
    ESP_LOGE(TAG, "Unknown WHO_AM_I: 0x%02X", who_am_i);
    this->mark_failed(LOG_STR("Unknown WHO_AM_I value"));
    return;
  }
  ESP_LOGD(TAG, "Found %s (WHO_AM_I = 0x%02X)", chip_name, who_am_i);
  this->chip_name_ = chip_name;

  // 2. Software reset — clears all registers to defaults
  if (this->write_register(LSM6DS_REG_CTRL3_C, &CTRL3_C_SW_RESET, 1) != i2c::ERROR_OK) {
    this->mark_failed(LOG_STR("Software reset failed"));
    return;
  }
  // Datasheet: reset bit self-clears after boot (typ. 50 µs);
  delay(2);

  // 3. Enable auto-increment and block data update (BDU).
  //    BDU prevents reading a high-byte from one sample and a low-byte from the next.
  //    IF_INC is set by default after reset but we set it explicitly for clarity.
  uint8_t ctrl3 = CTRL3_C_IF_INC | CTRL3_C_BDU;
  if (this->write_register(LSM6DS_REG_CTRL3_C, &ctrl3, 1) != i2c::ERROR_OK) {
    this->mark_failed(LOG_STR("Config failed"));
    return;
  }

  // 4. Configure accelerometer: ODR in bits[7:4], FS in bits[3:2]
  //    Anti-aliasing filter bandwidth is left at power-on default (bits[1:0] = 00 = ODR/2).
  uint8_t ctrl1_xl = (uint8_t) (this->accel_odr_ << 4) | (uint8_t) (this->accel_range_ << 2);
  if (this->write_register(LSM6DS_REG_CTRL1_XL, &ctrl1_xl, 1) != i2c::ERROR_OK) {
    this->mark_failed(LOG_STR("Failed to configure accelerometer"));
    return;
  }

  // 5. Configure gyroscope: ODR in bits[7:4], FS_G + FS_125 in bits[3:0]
  //    For ±125 dps: FS_G[2:1]=00 and FS_125(bit1)=1, so gyro_range_ encodes the full nibble.
  uint8_t ctrl2_g = (uint8_t) (this->gyro_odr_ << 4) | (uint8_t) (this->gyro_range_);
  if (this->write_register(LSM6DS_REG_CTRL2_G, &ctrl2_g, 1) != i2c::ERROR_OK) {
    this->mark_failed(LOG_STR("Failed to configure gyroscope"));
    return;
  }

  // 6. Ensure accelerometer is in high-performance mode (CTRL6_C bit 4 = XL_HM_MODE = 0)
  //    and gyroscope is in high-performance mode (CTRL7_G bit 7 = G_HM_MODE = 0).
  //    Both default to 0 (high-performance) after reset, but write explicitly.
  uint8_t zero = 0x00;
  if (this->write_register(LSM6DS_REG_CTRL6_C, &zero, 1) != i2c::ERROR_OK) {
    this->mark_failed();
    return;
  }
  if (this->write_register(LSM6DS_REG_CTRL7_G, &zero, 1) != i2c::ERROR_OK) {
    this->mark_failed();
    return;
  }
}

void LSM6DSComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "LSM6DS IMU:\n"
                "  Chip type: %s\n",
                this->chip_name_);
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);

  // Accel range — index into the sensitivity table (datasheet Table 3)
  static const char *const ACCEL_RANGE_STR[] = {"±2g", "±16g", "±4g", "±8g"};

  const char *gyro_str;
  switch (this->gyro_range_) {
    case LSM6DS_GYRO_RANGE_125:
      gyro_str = "±125dps";
      break;
    case LSM6DS_GYRO_RANGE_250:
      gyro_str = "±250dps";
      break;
    case LSM6DS_GYRO_RANGE_500:
      gyro_str = "±500dps";
      break;
    case LSM6DS_GYRO_RANGE_1000:
      gyro_str = "±1000dps";
      break;
    case LSM6DS_GYRO_RANGE_2000:
      gyro_str = "±2000dps";
      break;
    default:
      gyro_str = "unknown";
      break;
  }
  auto accel_odr = this->accel_odr_ == 0 ? 0 : 13 * (1 << (this->accel_odr_ - 1));
  auto gyro_odr = this->gyro_odr_ == 0 ? 0 : 13 * (1 << (this->gyro_odr_ - 1));
  ESP_LOGCONFIG(TAG,
                "  Accel range : %s\n"
                "  Accel data rate : %dHz\n"
                "  Gyro  range : %s\n"
                "  Gyro  data rate : %dHz",
                ACCEL_RANGE_STR[this->accel_range_], accel_odr, gyro_str, gyro_odr);
}

//  update_data()
// Called by MotionComponent::update() on each polling interval.
// Reads gyro XYZ and accel XYZ in a single 12-byte burst (registers 0x22–0x2D).
// Values are in g (accel) and °/s (gyro) — MotionComponent handles axis mapping
// and sensor publishing.

bool LSM6DSComponent::update_data(motion::MotionData &data) {
  if (this->is_failed())
    return false;

  // Single burst: gyro X/Y/Z (0x22–0x27) then accel X/Y/Z (0x28–0x2D)
  uint8_t raw[LSM6DS_BURST_LEN];
  if (!this->read_bytes(LSM6DS_REG_OUTX_L_G, raw, LSM6DS_BURST_LEN)) {
    this->status_set_error(LOG_STR("Failed to read IMU data"));
    return false;
  }
  this->status_clear_error();

  //  Gyroscope
  // Sensitivity (mdps/LSB) from datasheet Table 3.
  // Multiply by 1e-3 to convert mdps → dps (°/s).
  static constexpr float GYRO_SCALE[] = {
      8.75e-3f,   // 0x00 — ±250  dps
      8.75e-3f,   // 0x01 — unused (maps to 250 as fallback)
      4.375e-3f,  // 0x02 — ±125  dps  (FS_125 bit set)
      8.75e-3f,   // 0x03 — unused
      17.50e-3f,  // 0x04 — ±500  dps
      17.50e-3f,  // 0x05 — unused
      8.75e-3f,   // 0x06 — unused
      8.75e-3f,   // 0x07 — unused
      35.0e-3f,   // 0x08 — ±1000 dps
      35.0e-3f,   // 0x09 — unused
      17.50e-3f,  // 0x0A — unused
      17.50e-3f,  // 0x0B — unused
      70.0e-3f,   // 0x0C — ±2000 dps
  };
  float gyro_scale = GYRO_SCALE[this->gyro_range_];

  data.angular_rate[motion::X_AXIS] = (int16_t) ((raw[1] << 8) | raw[0]) * gyro_scale;
  data.angular_rate[motion::Y_AXIS] = (int16_t) ((raw[3] << 8) | raw[2]) * gyro_scale;
  data.angular_rate[motion::Z_AXIS] = (int16_t) ((raw[5] << 8) | raw[4]) * gyro_scale;

  //  Accelerometer
  // Sensitivity (mg/LSB) from datasheet Table 3.
  // Multiply by 1e-3 to convert mg → g.
  // Note: FS_XL register values are non-monotonic (0=2g, 1=16g, 2=4g, 3=8g).
  static constexpr float ACCEL_SCALE[] = {
      0.061e-3f,  // 0x00 — ±2g
      0.488e-3f,  // 0x01 — ±16g
      0.122e-3f,  // 0x02 — ±4g
      0.244e-3f,  // 0x03 — ±8g
  };
  float accel_scale = ACCEL_SCALE[this->accel_range_];

  data.acceleration[motion::X_AXIS] =
      (int16_t) ((raw[LSM6DS_ACCEL_OFFSET + 1] << 8) | raw[LSM6DS_ACCEL_OFFSET + 0]) * accel_scale;
  data.acceleration[motion::Y_AXIS] =
      (int16_t) ((raw[LSM6DS_ACCEL_OFFSET + 3] << 8) | raw[LSM6DS_ACCEL_OFFSET + 2]) * accel_scale;
  data.acceleration[motion::Z_AXIS] =
      (int16_t) ((raw[LSM6DS_ACCEL_OFFSET + 5] << 8) | raw[LSM6DS_ACCEL_OFFSET + 4]) * accel_scale;

  //  Temperature (lazy — only read if a listener is registered)
  // Kept as a separate 2-byte read to avoid extending the burst to 14 bytes when
  // temperature is not needed.
  // Formula: T(°C) = (raw / 256.0) + 25.0  (datasheet Table 90, OUT_TEMP register)
  if (!this->temperature_callback_.empty()) {
    uint8_t raw_t[2];
    if (this->read_bytes(LSM6DS_REG_OUT_TEMP_L, raw_t, 2)) {
      int16_t temp_raw = (int16_t) ((raw_t[1] << 8) | raw_t[0]);
      float temperature = (temp_raw / 256.0f) + 25.0f;
      this->temperature_callback_.call(temperature);
    }
  }

  return true;
}

}  // namespace esphome::lsm6ds
