#include "mpu6050.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mpu6050 {

static const char *const TAG = "mpu6050";

const uint8_t MPU6050_REGISTER_WHO_AM_I = 0x75;
const uint8_t MPU6050_REGISTER_POWER_MANAGEMENT_1 = 0x6B;
const uint8_t MPU6050_REGISTER_GYRO_CONFIG = 0x1B;
const uint8_t MPU6050_REGISTER_ACCEL_CONFIG = 0x1C;
const uint8_t MPU6050_REGISTER_ACCEL_XOUT_H = 0x3B;
const uint8_t MPU6050_CLOCK_SOURCE_X_GYRO = 0b001;
const uint8_t MPU6050_SCALE_2000_DPS = 0b11;
const float MPU6050_SCALE_DPS_PER_DIGIT_2000 = 0.060975f;
const uint8_t MPU6050_RANGE_2G = 0b00;
const float MPU6050_RANGE_PER_DIGIT_2G = 0.000061f;
const uint8_t MPU6050_BIT_SLEEP_ENABLED = 6;
const uint8_t MPU6050_BIT_TEMPERATURE_DISABLED = 3;
const uint8_t MPU6050_REGISTER_INT_ENABLE = 0x38;
const uint8_t MPU6050_REGISTER_MOTION_THRESHOLD = 0x1F;
const uint8_t MPU6050_REGISTER_MOTION_DURATION = 0x20;
const uint8_t MPU6050_BIT_MOTION_DET = 6;
const uint8_t MPU6050_REGISTER_INT_PIN_CFG = 0x37;
const uint8_t MPU6050_REGISTER_MOT_DETECT_CTRL = 0x69;
const uint8_t MPU6050_BIT_MOT_EN = 6;
const float GRAVITY_EARTH = 9.80665f;

void MPU6050Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MPU6050Component...");
  uint8_t who_am_i;
  if (!this->read_byte(MPU6050_REGISTER_WHO_AM_I, &who_am_i) ||
      (who_am_i != 0x68 && who_am_i != 0x70 && who_am_i != 0x98)) {
    this->mark_failed();
    return;
  }

  ESP_LOGV(TAG, "  Setting up Power Management...");
  // Setup power management
  uint8_t power_management;
  if (!this->read_byte(MPU6050_REGISTER_POWER_MANAGEMENT_1, &power_management)) {
    this->mark_failed();
    return;
  }
  ESP_LOGV(TAG, "  Input power_management: 0b" BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(power_management));
  // Set clock source - X-Gyro
  power_management &= 0b11111000;
  power_management |= MPU6050_CLOCK_SOURCE_X_GYRO;
  // Disable sleep
  power_management &= ~(1 << MPU6050_BIT_SLEEP_ENABLED);
  // Enable temperature
  power_management &= ~(1 << MPU6050_BIT_TEMPERATURE_DISABLED);
  ESP_LOGV(TAG, "  Output power_management: 0b" BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(power_management));
  if (!this->write_byte(MPU6050_REGISTER_POWER_MANAGEMENT_1, power_management)) {
    this->mark_failed();
    return;
  }

  ESP_LOGV(TAG, "  Setting up Gyro Config...");
  // Set scale - 2000DPS
  uint8_t gyro_config;
  if (!this->read_byte(MPU6050_REGISTER_GYRO_CONFIG, &gyro_config)) {
    this->mark_failed();
    return;
  }
  ESP_LOGV(TAG, "  Input gyro_config: 0b" BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(gyro_config));
  gyro_config &= 0b11100111;
  gyro_config |= MPU6050_SCALE_2000_DPS << 3;
  ESP_LOGV(TAG, "  Output gyro_config: 0b" BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(gyro_config));
  if (!this->write_byte(MPU6050_REGISTER_GYRO_CONFIG, gyro_config)) {
    this->mark_failed();
    return;
  }

  ESP_LOGV(TAG, "  Setting up Accel Config...");
  // Set range - 2G
  uint8_t accel_config;
  if (!this->read_byte(MPU6050_REGISTER_ACCEL_CONFIG, &accel_config)) {
    this->mark_failed();
    return;
  }
  ESP_LOGV(TAG, "    Input accel_config: 0b" BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(accel_config));
  accel_config &= 0b11100111;
  accel_config |= (MPU6050_RANGE_2G << 3);
  ESP_LOGV(TAG, "    Output accel_config: 0b" BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(accel_config));
  if (!this->write_byte(MPU6050_REGISTER_ACCEL_CONFIG, accel_config)) {
    this->mark_failed();
    return;
  }

  // Configure motion detection threshold and duration
  if (!this->write_byte(MPU6050_REGISTER_MOTION_THRESHOLD, 1)) {
    this->mark_failed();
    return;
  }
  if (!this->write_byte(MPU6050_REGISTER_MOTION_DURATION, 1)) {
    this->mark_failed();
    return;
  }

  // Configure motion detection control
  uint8_t mot_detect_ctrl = (1 << MPU6050_BIT_MOT_EN);
  if (!this->write_byte(MPU6050_REGISTER_MOT_DETECT_CTRL, mot_detect_ctrl)) {
    this->mark_failed();
    return;
  }

  // Enable interrupt latch and any read to clear
  uint8_t int_pin_cfg = 0x30;  // Enable latching and any read to clear
  if (!this->write_byte(MPU6050_REGISTER_INT_PIN_CFG, int_pin_cfg)) {
    this->mark_failed();
    return;
  }

  // Enable motion detection interrupt
  uint8_t int_enable = (1 << MPU6050_BIT_MOTION_DET);
  if (!this->write_byte(MPU6050_REGISTER_INT_ENABLE, int_enable)) {
    this->mark_failed();
    return;
  }
}

void MPU6050Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MPU6050:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with MPU6050 failed!");
  }
  ESP_LOGCONFIG(TAG, "Interrupt thresold: %d", 1);
}

float MPU6050Component::get_setup_priority() const { return setup_priority::DATA; }

}  // namespace mpu6050
}  // namespace esphome
