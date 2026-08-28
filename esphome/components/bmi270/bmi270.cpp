#include "bmi270.h"
#include "bmi270_config.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome::bmi270 {

static const char *const TAG = "bmi270";

#if defined(USE_ARDUINO) && !defined(USE_ESP32)
static const size_t MAX_I2C_BUFFER_SIZE = 32;
#else
static const size_t MAX_I2C_BUFFER_SIZE = 256;
#endif

//  Configuration blob upload
// The BMI270 requires a firmware config blob to be written to its internal
// memory after every power-on before sensors can be used.

bool BMI270Component::load_config_file_() {
  // 1. Disable advanced power-save so the config port is accessible
  if (!this->write_byte(BMI270_REG_PWR_CONF, 0x00))
    return false;
  delay(1);

  // 2. Prepare config load: write 0x00 to INIT_CTRL to start
  if (!this->write_byte(BMI270_REG_INIT_CTRL, 0x00))
    return false;

  // 3. Burst-write the config in pages
  const uint8_t *cfg = BMI270_CONFIG_FILE;
  constexpr size_t cfg_len = sizeof(BMI270_CONFIG_FILE);
  size_t index = 0;

  while (index != cfg_len) {
    // Set the page address in INIT_ADDR registers
    uint8_t addr_lsb = (uint8_t) ((index / 2) & 0x0F);
    uint8_t addr_msb = (uint8_t) ((index / 2) >> 4);
    if (!this->write_byte(BMI270_REG_INIT_ADDR_0, addr_lsb))
      return false;
    if (!this->write_byte(BMI270_REG_INIT_ADDR_0 + 1, addr_msb))
      return false;

    // Write a burst of up to the maximum allowed size
    size_t burst = clamp_at_most(cfg_len - index, MAX_I2C_BUFFER_SIZE);
    if (this->write_register(BMI270_REG_INIT_DATA, cfg + index, burst) != i2c::ERROR_OK)
      return false;

    index += burst;
  }

  // 4. Signal end of config load
  if (!this->write_byte(BMI270_REG_INIT_CTRL, 0x01))
    return false;
  delay(20);  // spec: wait ≥20 ms for init to complete

  // 5. Check INTERNAL_STATUS: bit[0:3] should be 0x01 ("initialisation OK")
  uint8_t status = 0;
  if (!this->read_byte(BMI270_REG_INTERNAL_STATUS, &status))
    return false;
  if ((status & 0x0F) != 0x01) {
    ESP_LOGE(TAG, "Config load failed: INTERNAL_STATUS=0x%02X (expected 0x01)", status);
    return false;
  }
  return true;
}

//  setup() ─

void BMI270Component::setup() {
  MotionComponent::setup();
  // 1. Verify chip ID
  uint8_t chip_id = 0;
  if (!this->read_byte(BMI270_REG_CHIP_ID, &chip_id)) {
    ESP_LOGE(TAG, "Failed to read chip ID – check wiring / address");
    this->mark_failed();
    return;
  }
  if (chip_id != BMI270_CHIP_ID_VALUE) {
    ESP_LOGE(TAG, "Wrong chip ID: 0x%02X (expected 0x%02X)", chip_id, BMI270_CHIP_ID_VALUE);
    this->mark_failed();
    return;
  }
  ESP_LOGD(TAG, "Chip ID: 0x%02X", chip_id);

  // 2. Soft-reset via CMD register (0x7E = 0xB6)
  if (!this->write_byte(0x7E, 0xB6)) {
    this->mark_failed();
    return;
  }
  delay(20);

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
  if (!this->write_byte(BMI270_REG_ACC_CONF, acc_conf)) {
    this->mark_failed();
    return;
  }
  if (!this->write_byte(BMI270_REG_ACC_RANGE, (uint8_t) accel_range_)) {
    this->mark_failed();
    return;
  }

  // 6. Configure gyroscope
  // GYR_CONF: ODR | BWP(0x2 = normal) | noise_perf(1) | filter_perf(1)
  uint8_t gyr_conf = (uint8_t) (gyro_odr_) | (0x2 << 4) | (1 << 6) | (1 << 7);
  if (!this->write_byte(BMI270_REG_GYR_CONF, gyr_conf)) {
    this->mark_failed();
    return;
  }
  if (!this->write_byte(BMI270_REG_GYR_RANGE, (uint8_t) gyro_range_)) {
    this->mark_failed();
    return;
  }

  // 7. Enable accelerometer, gyroscope, and temperature sensor
  //    PWR_CTRL bits: temp_en[3] | gyr_en[2] | acc_en[1]
  if (!this->write_byte(BMI270_REG_PWR_CTRL, 0x0E)) {
    this->mark_failed();
    return;
  }
  delay(5);

  // 8. Re-enable advanced power save (optional; keeps current low between reads)
  // Disabled here for simplicity – leave in performance mode
  if (!this->write_byte(BMI270_REG_PWR_CONF, 0x02)) {  // bit1 = fifo_self_wakeup
    this->mark_failed();
    return;
  }

  ESP_LOGCONFIG(TAG, "BMI270 initialised successfully");
}

void BMI270Component::dump_config() {
  ESP_LOGCONFIG(TAG, "BMI270 IMU:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Communication failed!");
    return;
  }

  static constexpr const char *const ACCEL_RANGE_STRS[] = {"±2g", "±4g", "±8g", "±16g"};
  static constexpr const char *const GYRO_RANGE_STRS[] = {"±2000°/s", "±1000°/s", "±500°/s", "±250°/s", "±125°/s"};

  ESP_LOGCONFIG(TAG, "  Accel range : %s", ACCEL_RANGE_STRS[accel_range_]);
  ESP_LOGCONFIG(TAG, "  Gyro  range : %s", GYRO_RANGE_STRS[gyro_range_]);
  MotionComponent::dump_config();
}

//  update() ─
// Reads all 6 axes + temperature in one block

bool BMI270Component::update_data(motion::MotionData &data) {
  if (this->is_failed())
    return false;

  //  Accelerometer: registers 0x0C–0x11 (6 bytes: x_lsb, x_msb, y_lsb, y_msb, z_lsb, z_msb)
  uint8_t raw_data[REG_READ_LEN];
  if (!this->read_bytes(BMI270_REG_DATA_8, raw_data, REG_READ_LEN)) {
    ESP_LOGW(TAG, "Failed to read IMU data");
    return false;
  }
  // Scale factor: LSB/g depends on range
  // raw is a signed 16-bit value; full-scale = range_g * 2^15 lsb
  static constexpr float ACCEL_SCALE[] = {
      2.0f / 32768.0f,
      4.0f / 32768.0f,
      8.0f / 32768.0f,
      16.0f / 32768.0f,
  };
  float scale = ACCEL_SCALE[this->accel_range_];

  data.acceleration[motion::X_AXIS] = (int16_t) ((raw_data[1] << 8) | raw_data[0]) * scale;
  data.acceleration[motion::Y_AXIS] = (int16_t) ((raw_data[3] << 8) | raw_data[2]) * scale;
  data.acceleration[motion::Z_AXIS] = (int16_t) ((raw_data[5] << 8) | raw_data[4]) * scale;

  // Gyroscope: registers 0x12–0x17 (6 bytes)
  // Scale: full-scale range / 2^15
  static constexpr float GYRO_SCALE[] = {
      2000.0f / 32768.0f, 1000.0f / 32768.0f, 500.0f / 32768.0f, 250.0f / 32768.0f, 125.0f / 32768.0f,
  };
  static constexpr uint8_t GYR_OFFS = BMI270_REG_DATA_14 - BMI270_REG_DATA_8;
  scale = GYRO_SCALE[this->gyro_range_];

  data.angular_rate[motion::X_AXIS] = (int16_t) ((raw_data[GYR_OFFS + 1] << 8) | raw_data[GYR_OFFS + 0]) * scale;
  data.angular_rate[motion::Y_AXIS] = (int16_t) ((raw_data[GYR_OFFS + 3] << 8) | raw_data[GYR_OFFS + 2]) * scale;
  data.angular_rate[motion::Z_AXIS] = (int16_t) ((raw_data[GYR_OFFS + 5] << 8) | raw_data[GYR_OFFS + 4]) * scale;

  if (this->temperature_callback_.empty())
    return true;
  //  Temperature: registers 0x22–0x23
  // Formula from datasheet: T[°C] = raw / 512 + 23
  static constexpr uint8_t TEMP_OFFS = BMI270_REG_TEMP_0 - BMI270_REG_DATA_8;
  int16_t raw_t = (int16_t) ((raw_data[TEMP_OFFS + 1] << 8) | raw_data[TEMP_OFFS + 0]);
  float temperature = (raw_t / 512.0f) + 23.0f;
  this->temperature_callback_.call(temperature);
  return true;
}

}  // namespace esphome::bmi270
