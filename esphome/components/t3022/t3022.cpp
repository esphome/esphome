#include "t3022.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace t3022 {

static const char *const TAG = "t3022";

// Command definitions based on T67xx/T3022 protocol
// Function codes: 0x03=Read Holding Registers, 0x04=Read Input Registers,
//                 0x05=Read Holding Registers (alternate), 0x06=Write Single Register

// Measure command: Read Holding Register 0x03F3, 1 word
static const uint8_t CMD_MEASURE[] = {0x05, 0x03, 0xF3, 0x00, 0x00};

// Read CO2 PPM: Read Input Register 0x138B, 1 word
static const uint8_t CMD_READ_CO2[] = {0x04, 0x13, 0x8B, 0x00, 0x01};

// Status register: Read Input Register 0x138A, 1 word
static const uint8_t CMD_STATUS[] = {0x04, 0x13, 0x8A, 0x00, 0x01};

bool T3022Component::send_command_(const uint8_t *command, size_t len) {
  i2c::ErrorCode err = this->write(command, len);
  if (err != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Failed to send command: %d", err);
    return false;
  }
  delay(READ_DELAY);
  return true;
}

bool T3022Component::read_response_(uint8_t *data, size_t len) {
  i2c::ErrorCode err = this->read(data, len);
  if (err != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Failed to read response: %d", err);
    return false;
  }
  return true;
}

bool T3022Component::check_status_() {
  uint8_t data[4];

  if (!send_command_(CMD_STATUS, sizeof(CMD_STATUS))) {
    return false;
  }

  if (!read_response_(data, sizeof(data))) {
    return false;
  }

  // Status is OK if data[0] == 0x04 && data[3] == 0x00 (per sample.ino)
  return (data[0] == 0x04 && data[3] == 0x00);
}

void T3022Component::read_co2_value_() {
  uint8_t data[4];

  // Read CO2 value
  if (!send_command_(CMD_READ_CO2, sizeof(CMD_READ_CO2))) {
    ESP_LOGE(TAG, "Failed to send read CO2 command");
    this->status_set_warning();
    this->publish_nan_();
    return;
  }

  if (!read_response_(data, 4)) {
    ESP_LOGE(TAG, "Failed to read CO2 response");
    this->status_set_warning();
    this->publish_nan_();
    return;
  }

  // CO2 value is in bytes 2-3, with upper 2 bits of byte 2 masked
  int16_t co2_value = ((data[2] & 0x3F) << 8) | data[3];

  if (this->co2_sensor_ != nullptr) {
    this->co2_sensor_->publish_state(co2_value);
  }
  this->status_clear_warning();
}

void T3022Component::publish_nan_() {
  if (this->co2_sensor_ != nullptr) {
    this->co2_sensor_->publish_state(NAN);
  }
}

void T3022Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up T3022...");

  // Check if sensor is responding by reading status register
  if (!this->check_status_()) {
    ESP_LOGE(TAG, "Communication with T3022 failed! Sensor not responding.");
    this->mark_failed();
    return;
  }

  ESP_LOGCONFIG(TAG, "T3022 sensor detected and responding");
}

void T3022Component::update() {
  if (this->co2_sensor_ == nullptr) {
    return;
  }

  uint8_t data[5];

  // Send measure command
  if (!send_command_(CMD_MEASURE, sizeof(CMD_MEASURE))) {
    ESP_LOGE(TAG, "Failed to send measure command");
    this->status_set_warning();
    this->publish_nan_();
    return;
  }

  // Read response to measure command
  if (!read_response_(data, sizeof(data))) {
    ESP_LOGE(TAG, "Failed to read measure response");
    this->status_set_warning();
    this->publish_nan_();
    return;
  }

  // Validate response - check if sensor responded correctly
  if (data[1] != 0x03 || data[4] != 0x00) {
    ESP_LOGE(TAG, "Measure command failed - invalid response");
    this->status_set_warning();
    this->publish_nan_();
    return;
  }

  // Schedule reading after measurement delay (non-blocking)
  this->set_timeout(MEASURE_DELAY, [this]() { this->read_co2_value_(); });
}

void T3022Component::dump_config() {
  ESP_LOGCONFIG(TAG, "T3022 CO2 Sensor:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with T3022 failed!");
  }
  LOG_SENSOR("  ", "CO2", this->co2_sensor_);
}

float T3022Component::get_setup_priority() const { return setup_priority::DATA; }

}  // namespace t3022
}  // namespace esphome
