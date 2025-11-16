#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "gl_r01_i2c.h"

namespace esphome {
namespace gl_r01_i2c {

static const char *const TAG = "gl_r01_i2c";

// Register definitions from datasheet
static const uint8_t REG_VERSION = 0x00;
static const uint8_t REG_DISTANCE = 0x02;
static const uint8_t REG_TRIGGER = 0x10;
static const uint8_t CMD_TRIGGER = 0xB0;
static const uint8_t RESTART_CMD1 = 0x5A;
static const uint8_t RESTART_CMD2 = 0xA5;
static const uint8_t READ_DELAY = 40;  // minimum milliseconds from datasheet to safely read measurement result

void GLR01I2CComponent::setup() {
  // Verify sensor presence
  if (!this->read_byte_16(REG_VERSION, &this->version_)) {
    ESP_LOGE(TAG, "Failed to communicate with GL-R01 I2C sensor!");
    this->mark_failed();
    return;
  }
  ESP_LOGD(TAG, "Found GL-R01 I2C with version 0x%04X", this->version_);
}

void GLR01I2CComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "GL-R01 I2C:");
  ESP_LOGCONFIG(TAG, " Firmware Version: 0x%04X", this->version_);
  LOG_I2C_DEVICE(this);
  LOG_SENSOR(" ", "Distance", this);
}

void GLR01I2CComponent::update() {
  // Trigger a new measurement
  if (!this->write_byte(REG_TRIGGER, CMD_TRIGGER)) {
    ESP_LOGE(TAG, "Failed to trigger measurement!");
    this->status_set_warning();
    return;
  }

  // Schedule reading the result after the read delay
  this->set_timeout(READ_DELAY, [this]() { this->read_distance_(); });
}

void GLR01I2CComponent::read_distance_() {
  uint16_t distance = 0;
  if (!this->read_byte_16(REG_DISTANCE, &distance)) {
    ESP_LOGE(TAG, "Failed to read distance value!");
    this->status_set_warning();
    return;
  }

  if (distance == 0xFFFF) {
    ESP_LOGW(TAG, "Invalid measurement received!");
    this->status_set_warning();
  } else {
    ESP_LOGV(TAG, "Distance: %umm", distance);
    this->publish_state(distance);
    this->status_clear_warning();
  }
}

}  // namespace gl_r01_i2c
}  // namespace esphome
