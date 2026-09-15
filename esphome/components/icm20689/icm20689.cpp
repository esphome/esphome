#include "icm20689.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::icm20689 {

static const char *const TAG = "icm20689";

void ICM20689Component::write_register_(uint8_t reg, uint8_t value) {
  this->enable();
  this->write_byte(reg & 0x7F);
  this->write_byte(value);
  this->disable();
}

void ICM20689Component::read_registers_(uint8_t reg, uint8_t *data, size_t len) {
  this->enable();
  this->write_byte(reg | ICM20689_READ_BIT);
  this->read_array(data, len);
  this->disable();
}

void ICM20689Component::setup() {
  MotionComponent::setup();

  this->spi_setup();

  this->enable();
  this->write_byte(ICM20689_REG_WHO_AM_I | ICM20689_READ_BIT);
  uint8_t who_am_i = this->transfer_byte(0x00);
  this->disable();
  if (who_am_i != ICM20689_WHO_AM_I_VALUE) {
    ESP_LOGE(TAG, "Unexpected WHO_AM_I 0x%02X (expected 0x%02X) -- check wiring/CS", who_am_i, ICM20689_WHO_AM_I_VALUE);
    this->mark_failed();
    return;
  }

  // Clear SLEEP, auto-select clock source (falls back to the internal oscillator if
  // the gyro PLL isn't ready yet).
  this->write_register_(ICM20689_REG_PWR_MGMT_1, 0x01);
  delay(50);  // NOLINT -- one-time settle time after waking from sleep, only runs in setup()
  this->write_register_(ICM20689_REG_ACCEL_CONFIG, this->accel_range_ << 3);
  this->write_register_(ICM20689_REG_GYRO_CONFIG, this->gyro_range_ << 3);
}

void ICM20689Component::dump_config() {
  ESP_LOGCONFIG(TAG, "ICM20689 IMU:");
  LOG_PIN("  CS Pin: ", this->cs_);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
    return;
  }

  static constexpr const char *const ACCEL_RANGE_STRS[] = {"±2g", "±4g", "±8g", "±16g"};
  static constexpr const char *const GYRO_RANGE_STRS[] = {"±250°/s", "±500°/s", "±1000°/s", "±2000°/s"};

  ESP_LOGCONFIG(TAG, "  Accel range: %s", ACCEL_RANGE_STRS[this->accel_range_]);
  ESP_LOGCONFIG(TAG, "  Gyro  range: %s", GYRO_RANGE_STRS[this->gyro_range_]);
  MotionComponent::dump_config();
}

bool ICM20689Component::update_data(motion::MotionData &data) {
  if (this->is_failed())
    return false;

  uint8_t raw_data[14];
  this->read_registers_(ICM20689_REG_ACCEL_XOUT_H, raw_data, sizeof(raw_data));

  auto to_int16 = [](uint8_t hi, uint8_t lo) -> int16_t { return static_cast<int16_t>((hi << 8) | lo); };

  // Scale factor: LSB/g depends on range -- full-scale = range_g * 2^15 lsb.
  static constexpr float ACCEL_SCALE[] = {
      2.0f / 32768.0f,
      4.0f / 32768.0f,
      8.0f / 32768.0f,
      16.0f / 32768.0f,
  };
  float accel_scale = ACCEL_SCALE[this->accel_range_];

  data.acceleration[motion::X_AXIS] = to_int16(raw_data[0], raw_data[1]) * accel_scale;
  data.acceleration[motion::Y_AXIS] = to_int16(raw_data[2], raw_data[3]) * accel_scale;
  data.acceleration[motion::Z_AXIS] = to_int16(raw_data[4], raw_data[5]) * accel_scale;

  // Scale factor: LSB/(deg/s) depends on range -- full-scale = range_dps * 2^15 lsb.
  static constexpr float GYRO_SCALE[] = {
      250.0f / 32768.0f,
      500.0f / 32768.0f,
      1000.0f / 32768.0f,
      2000.0f / 32768.0f,
  };
  float gyro_scale = GYRO_SCALE[this->gyro_range_];

  data.angular_rate[motion::X_AXIS] = to_int16(raw_data[8], raw_data[9]) * gyro_scale;
  data.angular_rate[motion::Y_AXIS] = to_int16(raw_data[10], raw_data[11]) * gyro_scale;
  data.angular_rate[motion::Z_AXIS] = to_int16(raw_data[12], raw_data[13]) * gyro_scale;

  if (!this->temperature_callback_.empty()) {
    // Datasheet formula: degC = raw / 326.8 + 25.
    float temperature = to_int16(raw_data[6], raw_data[7]) / 326.8f + 25.0f;
    this->temperature_callback_.call(temperature);
  }

  return true;
}

}  // namespace esphome::icm20689
