#include "bmi270.h"
#include "bmi270_config.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace bmi270 {

static const char *const TAG = "bmi270";

// ── Low-level I2C helpers ────────────────────────────────────────────────────

bool BMI270Component::write_reg_(uint8_t reg, uint8_t value) {
  return this->write_register(reg, &value, 1) == i2c::ERROR_OK;
}

bool BMI270Component::read_reg_(uint8_t reg, uint8_t *value) {
  return this->read_register(reg, value, 1) == i2c::ERROR_OK;
}

bool BMI270Component::read_bytes_(uint8_t reg, uint8_t *data, size_t len) {
  return this->read_register(reg, data, len) == i2c::ERROR_OK;
}

// ── Configuration blob upload ────────────────────────────────────────────────
// The BMI270 requires a firmware config blob to be written to its internal
// memory after every power-on before sensors can be used.

bool BMI270Component::load_config_file_() {
  // 1. Disable advanced power-save so the config port is accessible
  if (!write_reg_(BMI270_REG_PWR_CONF, 0x00))
    return false;
  delay(1);

  // 2. Prepare config load: write 0x00 to INIT_CTRL to start
  if (!write_reg_(BMI270_REG_INIT_CTRL, 0x00))
    return false;

  // 3. Burst-write the config in 256-byte pages
  const uint8_t *cfg = bmi270_config_file;
  const size_t cfg_len = sizeof(bmi270_config_file);
  size_t index = 0;

  while (index < cfg_len) {
    // Set the page address in INIT_ADDR registers
    uint8_t addr_lsb = (uint8_t) ((index / 2) & 0x0F);
    uint8_t addr_msb = (uint8_t) ((index / 2) >> 4);
    if (!write_reg_(BMI270_REG_INIT_ADDR_0, addr_lsb))
      return false;
    if (!write_reg_(BMI270_REG_INIT_ADDR_0 + 1, addr_msb))
      return false;

    // Write a burst of up to 256 bytes
    size_t burst = (cfg_len - index < 256) ? (cfg_len - index) : 256;
    if (this->write_register(BMI270_REG_INIT_DATA, cfg + index, burst) != i2c::ERROR_OK)
      return false;

    index += burst;
  }

  // 4. Signal end of config load
  if (!write_reg_(BMI270_REG_INIT_CTRL, 0x01))
    return false;
  delay(20);  // spec: wait ≥20 ms for init to complete

  // 5. Check INTERNAL_STATUS: bit[0:3] should be 0x01 ("initialisation OK")
  uint8_t status = 0;
  if (!read_reg_(BMI270_REG_INTERNAL_STATUS, &status))
    return false;
  if ((status & 0x0F) != 0x01) {
    ESP_LOGE(TAG, "Config load failed: INTERNAL_STATUS=0x%02X (expected 0x01)", status);
    return false;
  }
  return true;
}

// ── setup() ─────────────────────────────────────────────────────────────────

void BMI270Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up BMI270...");

  // 1. Verify chip ID
  uint8_t chip_id = 0;
  if (!read_reg_(BMI270_REG_CHIP_ID, &chip_id)) {
    ESP_LOGE(TAG, "Failed to read chip ID – check wiring / address");
    this->mark_failed();
    return;
  }
  if (chip_id != BMI270_CHIP_ID_VALUE) {
    ESP_LOGE(TAG, "Wrong chip ID: 0x%02X (expected 0x%02X)", chip_id, BMI270_CHIP_ID_VALUE);
    this->mark_failed();
    return;
  }
  ESP_LOGD(TAG, "Chip ID: 0x%02X ✓", chip_id);

  // 2. Soft-reset via CMD register (0x7E = 0xB6)
  if (!write_reg_(0x7E, 0xB6)) {
    this->mark_failed();
    return;
  }
  delay(20);

  // 3. Dummy read (required after reset on SPI; harmless on I2C)
  read_reg_(BMI270_REG_CHIP_ID, &chip_id);

  // 4. Upload the configuration blob
  if (!load_config_file_()) {
    ESP_LOGE(TAG, "Config file upload failed");
    this->mark_failed();
    return;
  }
  ESP_LOGD(TAG, "Config blob uploaded ✓");

  // 5. Configure accelerometer
  // ACC_CONF: ODR | BWP(0x2 = normal avg4) | perf_mode(1)
  uint8_t acc_conf = (uint8_t) (accel_odr_) | (0x2 << 4) | (1 << 7);
  if (!write_reg_(BMI270_REG_ACC_CONF, acc_conf)) {
    this->mark_failed();
    return;
  }
  if (!write_reg_(BMI270_REG_ACC_RANGE, (uint8_t) accel_range_)) {
    this->mark_failed();
    return;
  }

  // 6. Configure gyroscope
  // GYR_CONF: ODR | BWP(0x2 = normal) | noise_perf(1) | filter_perf(1)
  uint8_t gyr_conf = (uint8_t) (gyro_odr_) | (0x2 << 4) | (1 << 6) | (1 << 7);
  if (!write_reg_(BMI270_REG_GYR_CONF, gyr_conf)) {
    this->mark_failed();
    return;
  }
  if (!write_reg_(BMI270_REG_GYR_RANGE, (uint8_t) gyro_range_)) {
    this->mark_failed();
    return;
  }

  // 7. Enable accelerometer, gyroscope, and temperature sensor
  //    PWR_CTRL bits: temp_en[3] | gyr_en[2] | acc_en[1]
  if (!write_reg_(BMI270_REG_PWR_CTRL, 0x0E)) {
    this->mark_failed();
    return;
  }
  delay(5);

  // 8. Re-enable advanced power save (optional; keeps current low between reads)
  // Disabled here for simplicity – leave in performance mode
  if (!write_reg_(BMI270_REG_PWR_CONF, 0x02)) {  // bit1 = fifo_self_wakeup
    this->mark_failed();
    return;
  }

  init_ok_ = true;
  ESP_LOGCONFIG(TAG, "BMI270 initialised successfully");
}

// ── dump_config() ────────────────────────────────────────────────────────────

void BMI270Component::dump_config() {
  ESP_LOGCONFIG(TAG, "BMI270 IMU:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Communication failed!");
    return;
  }

  static const char *const accel_range_strs[] = {"±2g", "±4g", "±8g", "±16g"};
  static const char *const gyro_range_strs[] = {"±2000°/s", "±1000°/s", "±500°/s", "±250°/s", "±125°/s"};

  ESP_LOGCONFIG(TAG, "  Accel range : %s", accel_range_strs[accel_range_]);
  ESP_LOGCONFIG(TAG, "  Gyro  range : %s", gyro_range_strs[gyro_range_]);
  LOG_SENSOR("  ", "Accel X", accel_x_);
  LOG_SENSOR("  ", "Accel Y", accel_y_);
  LOG_SENSOR("  ", "Accel Z", accel_z_);
  LOG_SENSOR("  ", "Gyro X", gyro_x_);
  LOG_SENSOR("  ", "Gyro Y", gyro_y_);
  LOG_SENSOR("  ", "Gyro Z", gyro_z_);
  LOG_SENSOR("  ", "Temperature", temperature_);
}

// ── update() ─────────────────────────────────────────────────────────────────
// Reads all 6 axes + temperature in two consecutive burst reads.

void BMI270Component::update() {
  if (!init_ok_)
    return;

  // ── Accelerometer: registers 0x0C–0x11 (6 bytes: x_lsb, x_msb, y_lsb, y_msb, z_lsb, z_msb)
  if (accel_x_ || accel_y_ || accel_z_) {
    uint8_t raw_acc[6];
    if (!read_bytes_(BMI270_REG_DATA_8, raw_acc, 6)) {
      ESP_LOGW(TAG, "Failed to read accelerometer data");
    } else {
      // Scale factor: LSB/g depends on range
      // raw is a signed 16-bit value; full-scale = range_g * 2^15 lsb
      static const float accel_scale[] = {
          2.0f / 32768.0f,
          4.0f / 32768.0f,
          8.0f / 32768.0f,
          16.0f / 32768.0f,
      };
      float scale = accel_scale[accel_range_];

      int16_t ax = (int16_t) ((raw_acc[1] << 8) | raw_acc[0]);
      int16_t ay = (int16_t) ((raw_acc[3] << 8) | raw_acc[2]);
      int16_t az = (int16_t) ((raw_acc[5] << 8) | raw_acc[4]);

      if (accel_x_)
        accel_x_->publish_state(ax * scale);
      if (accel_y_)
        accel_y_->publish_state(ay * scale);
      if (accel_z_)
        accel_z_->publish_state(az * scale);
    }
  }

  // ── Gyroscope: registers 0x12–0x17 (6 bytes)
  if (gyro_x_ || gyro_y_ || gyro_z_) {
    uint8_t raw_gyr[6];
    if (!read_bytes_(BMI270_REG_DATA_14, raw_gyr, 6)) {
      ESP_LOGW(TAG, "Failed to read gyroscope data");
    } else {
      // Scale: full-scale range / 2^15
      static const float gyro_scale[] = {
          2000.0f / 32768.0f, 1000.0f / 32768.0f, 500.0f / 32768.0f, 250.0f / 32768.0f, 125.0f / 32768.0f,
      };
      float scale = gyro_scale[gyro_range_];

      int16_t gx = (int16_t) ((raw_gyr[1] << 8) | raw_gyr[0]);
      int16_t gy = (int16_t) ((raw_gyr[3] << 8) | raw_gyr[2]);
      int16_t gz = (int16_t) ((raw_gyr[5] << 8) | raw_gyr[4]);

      if (gyro_x_)
        gyro_x_->publish_state(gx * scale);
      if (gyro_y_)
        gyro_y_->publish_state(gy * scale);
      if (gyro_z_)
        gyro_z_->publish_state(gz * scale);
    }
  }

  // ── Temperature: registers 0x22–0x23
  // Formula from datasheet: T[°C] = raw / 512 + 23
  if (temperature_) {
    uint8_t raw_temp[2];
    if (!read_bytes_(BMI270_REG_TEMP_0, raw_temp, 2)) {
      ESP_LOGW(TAG, "Failed to read temperature");
    } else {
      int16_t raw_t = (int16_t) ((raw_temp[1] << 8) | raw_temp[0]);
      float temp_c = (raw_t / 512.0f) + 23.0f;
      temperature_->publish_state(temp_c);
    }
  }
}

}  // namespace bmi270
}  // namespace esphome
