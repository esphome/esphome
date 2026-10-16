#include "icm20689.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <cstddef>
#include <cstdint>

namespace esphome::icm20689 {

ESPHOME_LOG_TAG(TAG, "icm20689");

namespace {

// Range enum values 0..3 map to full-scale settings of 2x/4x/8x/16x the base range.
constexpr float accel_scale(uint8_t range) { return (2.0f * (1U << range)) / 32768.0f; }

constexpr float gyro_scale(uint8_t range) { return (250.0f * (1U << range)) / 32768.0f; }

}  // namespace

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

  if (!this->spi_is_ready()) {
    ESP_LOGE(TAG, "SPI device is not ready");
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }

  // Datasheet 4.20: a soft reset must be the first operation after power-up,
  // before any register initialization. WHO_AM_I is only valid afterwards.
  this->write_register_(ICM20689_REG_PWR_MGMT_1, ICM20689_SOFT_RESET);
  delay(100);  // NOLINT

  // The chip comes out of reset in sleep mode: wake it and select the
  // auto-selected best clock source (required for full gyro performance).
  this->write_register_(ICM20689_REG_PWR_MGMT_1, ICM20689_CLKSEL_AUTO);

  // USER_CTRL is not writable in sleep mode, so I2C_IF_DIS is set once awake.
  // Datasheet 6.1: disable the I2C interface when using SPI.
  this->write_register_(ICM20689_REG_USER_CTRL, ICM20689_I2C_IF_DIS);

  uint8_t who_am_i = 0;
  this->read_registers_(ICM20689_REG_WHO_AM_I, &who_am_i, 1);

  if (who_am_i != ICM20689_WHO_AM_I_VALUE) {
    ESP_LOGE(TAG, "Unexpected WHO_AM_I 0x%02X (expected 0x%02X) -- check wiring/CS", who_am_i, ICM20689_WHO_AM_I_VALUE);
    this->mark_failed(LOG_STR("Wrong chip ID"));
    return;
  }

  // Allow the sensor to settle after waking from sleep.
  delay(50);  // NOLINT

  // Configure the accelerometer and gyroscope full-scale ranges.
  this->write_register_(ICM20689_REG_ACCEL_CONFIG, static_cast<uint8_t>(this->accel_range_) << 3);
  this->write_register_(ICM20689_REG_GYRO_CONFIG, static_cast<uint8_t>(this->gyro_range_) << 3);
}

void ICM20689Component::dump_config() {
  ESP_LOGCONFIG(TAG, "ICM20689 IMU:");
  LOG_PIN("  CS Pin: ", this->cs_);

  if (this->is_failed())
    return;

  ESP_LOGCONFIG(TAG, "  Accel range: ±%dg", 2 << static_cast<uint8_t>(this->accel_range_));
  ESP_LOGCONFIG(TAG, "  Gyro range: ±%d°/s", 250 << static_cast<uint8_t>(this->gyro_range_));

  MotionComponent::dump_config();
}

bool ICM20689Component::update_data(motion::MotionData &data) {
  if (this->is_failed())
    return false;

  const uint8_t accel_range = static_cast<uint8_t>(this->accel_range_);
  const uint8_t gyro_range = static_cast<uint8_t>(this->gyro_range_);

  // Read one contiguous sample:
  // 0..5   accelerometer X/Y/Z
  // 6..7   temperature
  // 8..13  gyroscope X/Y/Z
  uint8_t raw_data[14] = {};
  this->read_registers_(ICM20689_REG_ACCEL_XOUT_H, raw_data, sizeof(raw_data));

  const float accel_factor = accel_scale(accel_range);

  data.acceleration[motion::X_AXIS] = static_cast<int16_t>(encode_uint16(raw_data[0], raw_data[1])) * accel_factor;
  data.acceleration[motion::Y_AXIS] = static_cast<int16_t>(encode_uint16(raw_data[2], raw_data[3])) * accel_factor;
  data.acceleration[motion::Z_AXIS] = static_cast<int16_t>(encode_uint16(raw_data[4], raw_data[5])) * accel_factor;

  const float gyro_factor = gyro_scale(gyro_range);

  data.angular_rate[motion::X_AXIS] = static_cast<int16_t>(encode_uint16(raw_data[8], raw_data[9])) * gyro_factor;
  data.angular_rate[motion::Y_AXIS] = static_cast<int16_t>(encode_uint16(raw_data[10], raw_data[11])) * gyro_factor;
  data.angular_rate[motion::Z_AXIS] = static_cast<int16_t>(encode_uint16(raw_data[12], raw_data[13])) * gyro_factor;

  if (!this->temperature_callback_.empty()) {
    // Datasheet formula: temperature (°C) = raw / 326.8 + 25.
    const float temperature = static_cast<int16_t>(encode_uint16(raw_data[6], raw_data[7])) / 326.8f + 25.0f;
    this->temperature_callback_.call(temperature);
  }

  return true;
}

}  // namespace esphome::icm20689
