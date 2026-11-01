#include "lsm6ds.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/progmem.h"

namespace esphome::lsm6ds {

ESPHOME_LOG_TAG(TAG, "lsm6ds");

// Indexed by the accel range field, which is not in ascending order (datasheet Table 3)
PROGMEM_STRING_TABLE(AccelRangeStrings, "±2g", "±16g", "±4g", "±8g");

static const LogString *gyro_range_to_str(LSM6DSGyroRange range) {
  switch (range) {
    case LSM6DS_GYRO_RANGE_125:
      return LOG_STR("±125dps");
    case LSM6DS_GYRO_RANGE_250:
      return LOG_STR("±250dps");
    case LSM6DS_GYRO_RANGE_500:
      return LOG_STR("±500dps");
    case LSM6DS_GYRO_RANGE_1000:
      return LOG_STR("±1000dps");
    case LSM6DS_GYRO_RANGE_2000:
      return LOG_STR("±2000dps");
    default:
      return LOG_STR("unknown");
  }
}

const LogString *chip_name_from_who_am_i(uint8_t who_am_i) {
  switch (who_am_i) {
    case LSM6DS_WHO_AM_I_LSM6DS3:
      return LOG_STR("LSM6DS3");
    case LSM6DS_WHO_AM_I_LSM6DS3TR_C:
      return LOG_STR("LSM6DS3TR-C");
    case LSM6DS_WHO_AM_I_LSM6DSO:
      return LOG_STR("LSM6DSO");
    default:
      return nullptr;
  }
}

uint16_t temperature_sensitivity_from_who_am_i(uint8_t who_am_i) {
  return who_am_i == LSM6DS_WHO_AM_I_LSM6DS3 ? 16 : 256;
}

void LSM6DSComponent::setup() {
  MotionComponent::setup();
  uint8_t who_am_i = 0;
  if (this->read_register(LSM6DS_REG_WHO_AM_I, &who_am_i, 1) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Failed to read WHO_AM_I — check wiring and I2C address");
    this->mark_failed();
    return;
  }
  const LogString *chip_name = chip_name_from_who_am_i(who_am_i);
  if (chip_name == nullptr) {
    ESP_LOGE(TAG, "Unknown WHO_AM_I: 0x%02X", who_am_i);
    this->mark_failed(LOG_STR("Unknown WHO_AM_I value"));
    return;
  }
  ESP_LOGD(TAG, "Found %s (WHO_AM_I = 0x%02X)", LOG_STR_ARG(chip_name), who_am_i);
  this->chip_name_ = chip_name;
  this->temperature_sensitivity_ = temperature_sensitivity_from_who_am_i(who_am_i);

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
                LOG_STR_ARG(this->chip_name_ != nullptr ? this->chip_name_ : LOG_STR("Unknown")));
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);

  auto accel_odr = this->accel_odr_ == 0 ? 0 : 13 * (1 << (this->accel_odr_ - 1));
  auto gyro_odr = this->gyro_odr_ == 0 ? 0 : 13 * (1 << (this->gyro_odr_ - 1));
  ESP_LOGCONFIG(TAG,
                "  Accel range : %s\n"
                "  Accel data rate : %dHz\n"
                "  Gyro  range : %s\n"
                "  Gyro  data rate : %dHz",
                LOG_STR_ARG(AccelRangeStrings::get_log_str(this->accel_range_, AccelRangeStrings::LAST_INDEX)),
                accel_odr, LOG_STR_ARG(gyro_range_to_str(this->gyro_range_)), gyro_odr);
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

  data.angular_rate[motion::X_AXIS] = encode_value_le<int16_t>(raw) * gyro_scale;
  data.angular_rate[motion::Y_AXIS] = encode_value_le<int16_t>(&raw[2]) * gyro_scale;
  data.angular_rate[motion::Z_AXIS] = encode_value_le<int16_t>(&raw[4]) * gyro_scale;

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

  data.acceleration[motion::X_AXIS] = encode_value_le<int16_t>(&raw[LSM6DS_ACCEL_OFFSET]) * accel_scale;
  data.acceleration[motion::Y_AXIS] = encode_value_le<int16_t>(&raw[LSM6DS_ACCEL_OFFSET + 2]) * accel_scale;
  data.acceleration[motion::Z_AXIS] = encode_value_le<int16_t>(&raw[LSM6DS_ACCEL_OFFSET + 4]) * accel_scale;

#ifdef USE_MOTION_TEMPERATURE_PUBLISHER
  //  Temperature (lazy — only read if a sensor is configured)
  // Kept as a separate 2-byte read to avoid extending the burst to 14 bytes when
  // temperature is not needed.
  // Formula: T(°C) = raw / sensitivity + 25.0, sensitivity in LSB/°C per chip (0 LSB at 25 °C)
  if (this->temperature_publisher_ != nullptr) {
    uint8_t raw_t[2];
    if (this->read_bytes(LSM6DS_REG_OUT_TEMP_L, raw_t, 2)) {
      int16_t temp_raw = encode_value_le<int16_t>(raw_t);
      float temperature = temp_raw / static_cast<float>(this->temperature_sensitivity_) + 25.0f;
      this->temperature_publisher_(temperature);
    }
  }
#endif

  return true;
}

}  // namespace esphome::lsm6ds
