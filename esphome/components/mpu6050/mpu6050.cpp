#include "mpu6050.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace mpu6050 {

static const char *const TAG = "mpu6050";

void MPU6050Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MPU6050 component...");

  uint8_t who_am_i;
  if (!this->read_byte(MPU6050_REGISTER_WHO_AM_I, &who_am_i) ||
      (who_am_i != 0x68 && who_am_i != 0x70 && who_am_i != 0x98)) {
    ESP_LOGE(TAG, "  Failed to know who am I");
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
  // Disable sleep
  power_management &= ~(1 << MPU6050_BIT_SLEEP_ENABLED);
  ESP_LOGV(TAG, "  Output power_management: 0b" BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(power_management));
  if (!this->write_byte(MPU6050_REGISTER_POWER_MANAGEMENT_1, power_management)) {
    this->mark_failed();
    return;
  }

#ifdef USE_MPU6050_INTERRUPT
  // Configure motion detection threshold and duration
  // https://github.com/jrowberg/i2cdevlib/blob/19c420b6798872ee91e85fc5f522fbc5b8771194/Arduino/MPU6050/MPU6050.cpp#L496-L526
  if (!this->write_byte(MPU6050_REGISTER_MOTION_THRESHOLD, this->threshold_)) {
    ESP_LOGE(TAG, "  Failed to set motion interrupt threshold to %d", this->threshold_);
    this->mark_failed();
    // return;
  }
  // https://github.com/jrowberg/i2cdevlib/blob/19c420b6798872ee91e85fc5f522fbc5b8771194/Arduino/MPU6050/MPU6050.cpp#L530-L556
  if (!this->write_byte(MPU6050_REGISTER_MOTION_DURATION, this->duration_)) {
    ESP_LOGE(TAG, "  Failed to set motion interrupt duration to %d", this->duration_);
    this->mark_failed();
    // return;
  }

  // Configure motion detection control
  uint8_t mot_detect_ctrl = (1 << MPU6050_BIT_MOT_EN);
  if (!this->write_byte(MPU6050_REGISTER_MOT_DETECT_CTRL, mot_detect_ctrl)) {
    ESP_LOGE(TAG, "  Failed to configure interrupt motion detection control");
    this->mark_failed();
    // return;
  }

  // Enable interrupt latch and any read to clear
  uint8_t int_pin_cfg = 0x30;  // Enable latching and any read to clear
  if (!this->write_byte(MPU6050_REGISTER_INT_PIN_CFG, int_pin_cfg)) {
    ESP_LOGE(TAG, "  Failed to enable interrupt latch and any read to clear");
    this->mark_failed();
    // return;
  }

  // Enable motion detection interrupt
  uint8_t int_enable = (1 << MPU6050_BIT_MOTION_DET);
  if (!this->write_byte(MPU6050_REGISTER_INT_ENABLE, int_enable)) {
    ESP_LOGE(TAG, "  Failed to enable motion detection interrupt");
    this->mark_failed();
    // return;
  }
#endif  // USE_MPU6050_INTERRUPT
}

void MPU6050Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MPU6050:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Communication with MPU6050 failed!");
  }
  ESP_LOGCONFIG(TAG, "  Interrupt thresold: %d", 1);
}

float MPU6050Component::get_setup_priority() const { return setup_priority::DATA; }

#ifdef USE_MPU6050_INTERRUPT
void MPU6050Component::set_interrupt(uint8_t threshold, uint8_t duration) {
  this->threshold_ = threshold;
  this->duration_ = duration;
}
#endif  // USE_MPU6050_INTERRUPT

}  // namespace mpu6050
}  // namespace esphome
