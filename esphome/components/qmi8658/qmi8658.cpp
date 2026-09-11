#include "qmi8658.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome::qmi8658 {

static const char *const TAG = "qmi8658";

// Acceleration scale (g per LSB), indexed by accel_range_ >> 4.
// Full-scale = range_g, mapped over a signed 16-bit value (2^15 counts).
static constexpr float ACCEL_SCALE[] = {
    2.0f / 32768.0f,
    4.0f / 32768.0f,
    8.0f / 32768.0f,
    16.0f / 32768.0f,
};

// Angular rate scale (°/s per LSB), indexed by gyro_range_ >> 4.
static constexpr float GYRO_SCALE[] = {
    16.0f / 32768.0f,  32.0f / 32768.0f,  64.0f / 32768.0f,   128.0f / 32768.0f,
    256.0f / 32768.0f, 512.0f / 32768.0f, 1024.0f / 32768.0f, 2048.0f / 32768.0f,
};

void QMI8658Component::setup() {
  MotionComponent::setup();

  // 1. Verify chip ID
  uint8_t who_am_i = 0;
  if (!this->read_byte(QMI8658_REG_WHO_AM_I, &who_am_i)) {
    ESP_LOGE(TAG, "Failed to read chip ID - check wiring / address");
    this->mark_failed();
    return;
  }
  if (who_am_i != QMI8658_WHO_AM_I_VALUE) {
    ESP_LOGE(TAG, "Wrong chip ID: 0x%02X (expected 0x%02X)", who_am_i, QMI8658_WHO_AM_I_VALUE);
    this->mark_failed();
    return;
  }

  // 2. Soft reset
  if (!this->write_byte(QMI8658_REG_RESET, QMI8658_RESET_CMD)) {
    this->mark_failed();
    return;
  }
  delay(15);  // spec: wait for reset to complete

  // 3. Serial interface: enable register address auto-increment
  if (!this->write_byte(QMI8658_REG_CTRL1, QMI8658_CTRL1_VALUE)) {
    this->mark_failed(LOG_STR("Failed to write REG_CTRL1"));
    return;
  }

  // 4. Configure accelerometer (CTRL2 = range | ODR)
  if (!this->write_byte(QMI8658_REG_CTRL2, (uint8_t) (this->accel_range_) | (uint8_t) (this->accel_odr_))) {
    this->mark_failed(LOG_STR("Failed to write REG_CTRL2"));
    return;
  }

  // 5. Configure gyroscope (CTRL3 = range | ODR)
  if (!this->write_byte(QMI8658_REG_CTRL3, (uint8_t) (this->gyro_range_) | (uint8_t) (this->gyro_odr_))) {
    this->mark_failed(LOG_STR("Failed to write REG_CTRL3"));
    return;
  }

  // 6. Disable the built-in low-pass filters (leave raw data to the motion pipeline)
  if (!this->write_byte(QMI8658_REG_CTRL5, 0x00)) {
    this->mark_failed(LOG_STR("Failed to write REG_CTRL5"));
    this->mark_failed();
    return;
  }

  // 7. Enable accelerometer and gyroscope
  if (!this->write_byte(QMI8658_REG_CTRL7, QMI8658_CTRL7_ACC_EN | QMI8658_CTRL7_GYR_EN)) {
    this->mark_failed(LOG_STR("Failed to write REG_CTRL7"));
    return;
  }

  ESP_LOGCONFIG(TAG, "QMI8658 initialised successfully");
}

void QMI8658Component::dump_config() {
  ESP_LOGCONFIG(TAG, "QMI8658 IMU:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Communication failed!");
    return;
  }

  static constexpr const char *const ACCEL_RANGE_STRS[] = {"±2g", "±4g", "±8g", "±16g"};
  static constexpr const char *const GYRO_RANGE_STRS[] = {"±16°/s",  "±32°/s",  "±64°/s",   "±128°/s",
                                                          "±256°/s", "±512°/s", "±1024°/s", "±2048°/s"};

  ESP_LOGCONFIG(TAG, "  Accel range : %s", ACCEL_RANGE_STRS[this->accel_range_ >> 4]);
  ESP_LOGCONFIG(TAG, "  Gyro  range : %s", GYRO_RANGE_STRS[this->gyro_range_ >> 4]);
  MotionComponent::dump_config();
}

bool QMI8658Component::update_data(motion::MotionData &data) {
  if (this->is_failed())
    return false;

  // Read temperature + accel + gyro in one contiguous block starting at TEMP_L.
  uint8_t raw_data[REG_READ_LEN];
  if (!this->read_bytes(QMI8658_REG_TEMP_L, raw_data, REG_READ_LEN)) {
    ESP_LOGW(TAG, "Failed to read IMU data");
    return false;
  }

  // Data is little-endian (low byte first).
  float scale = ACCEL_SCALE[this->accel_range_ >> 4];
  int16_t raw_x = encode_uint16(raw_data[ACC_OFFS + 1], raw_data[ACC_OFFS + 0]);
  int16_t raw_y = encode_uint16(raw_data[ACC_OFFS + 3], raw_data[ACC_OFFS + 2]);
  int16_t raw_z = encode_uint16(raw_data[ACC_OFFS + 5], raw_data[ACC_OFFS + 4]);
  ESP_LOGV(TAG, "Read raw accel data: %d, %d, %d", raw_x, raw_y, raw_z);
  data.acceleration[motion::X_AXIS] = raw_x * scale;
  data.acceleration[motion::Y_AXIS] = raw_y * scale;
  data.acceleration[motion::Z_AXIS] = raw_z * scale;

  scale = GYRO_SCALE[this->gyro_range_ >> 4];
  raw_x = encode_uint16(raw_data[GYR_OFFS + 1], raw_data[GYR_OFFS + 0]);
  raw_y = encode_uint16(raw_data[GYR_OFFS + 3], raw_data[GYR_OFFS + 2]);
  raw_z = encode_uint16(raw_data[GYR_OFFS + 5], raw_data[GYR_OFFS + 4]);
  ESP_LOGV(TAG, "Read raw gyro data: %d, %d, %d", raw_x, raw_y, raw_z);
  data.angular_rate[motion::X_AXIS] = raw_x * scale;
  data.angular_rate[motion::Y_AXIS] = raw_y * scale;
  data.angular_rate[motion::Z_AXIS] = raw_z * scale;

  if (this->temperature_callback_.empty())
    return true;
  // Temperature: signed 16-bit, °C = raw / 256
  int16_t raw_t = (int16_t) ((raw_data[TEMP_OFFS + 1] << 8) | raw_data[TEMP_OFFS + 0]);
  this->temperature_callback_.call(raw_t / 256.0f);
  return true;
}

}  // namespace esphome::qmi8658
