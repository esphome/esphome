#include "kmeteriso.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace kmeteriso {

static const char *const TAG = "kmeteriso.sensor";

static const uint8_t KMETER_ERROR_STATUS_REG = 0x20;
static const uint8_t KMETER_TEMP_VAL_REG = 0x00;
static const uint8_t KMETER_INTERNAL_TEMP_VAL_REG = 0x10;
static const uint8_t KMETER_FIRMWARE_VERSION_REG = 0xFE;

void KMeterISOComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up KMeterISO...");
  this->error_code_ = NONE;

  // Mark as not failed before initializing. Some devices will turn off sensors to save on batteries
  // and when they come back on, the COMPONENT_STATE_FAILED bit must be unset on the component.
  if ((this->component_state_ & COMPONENT_STATE_MASK) == COMPONENT_STATE_FAILED) {
    this->component_state_ &= ~COMPONENT_STATE_MASK;
    this->component_state_ |= COMPONENT_STATE_CONSTRUCTION;
  }

  auto err = this->bus_->writev(this->address_, nullptr, 0);
  if (err == esphome::i2c::ERROR_OK) {
    ESP_LOGCONFIG(TAG, "Could write to the address %d.", this->address_);
  } else {
    ESP_LOGCONFIG(TAG, "Could not write to the address %d.", this->address_);
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }

  uint8_t read_buf[4] = {1};
  if (!this->read_bytes(KMETER_ERROR_STATUS_REG, read_buf, 1)) {
    ESP_LOGCONFIG(TAG, "Could not read from the device.");
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }
  if (read_buf[0] != 0) {
    ESP_LOGCONFIG(TAG, "The device is not ready.");
    this->error_code_ = STATUS_FAILED;
    this->mark_failed();
    return;
  }
  ESP_LOGCONFIG(TAG, "The device was successfully setup.");
}

float KMeterISOComponent::get_setup_priority() const { return setup_priority::DATA; }

void KMeterISOComponent::update() {
  uint8_t read_buf[4];

  if (this->temperature_sensor_ != nullptr) {
    if (!this->read_bytes(KMETER_TEMP_VAL_REG, read_buf, 4)) {
      ESP_LOGW(TAG, "Error reading temperature.");
    } else {
      int32_t temp = encode_uint32(read_buf[3], read_buf[2], read_buf[1], read_buf[0]);
      float temp_f = temp / 100.0;
      ESP_LOGV(TAG, "Got temperature=%.2f °C", temp_f);
      this->temperature_sensor_->publish_state(temp_f);
    }
  }

  if (this->internal_temperature_sensor_ != nullptr) {
    if (!this->read_bytes(KMETER_INTERNAL_TEMP_VAL_REG, read_buf, 4)) {
      ESP_LOGW(TAG, "Error reading internal temperature.");
      return;
    } else {
      int32_t internal_temp = encode_uint32(read_buf[3], read_buf[2], read_buf[1], read_buf[0]);
      float internal_temp_f = internal_temp / 100.0;
      ESP_LOGV(TAG, "Got internal temperature=%.2f °C", internal_temp_f);
      this->internal_temperature_sensor_->publish_state(internal_temp_f);
    }
  }
}

}  // namespace kmeteriso
}  // namespace esphome
