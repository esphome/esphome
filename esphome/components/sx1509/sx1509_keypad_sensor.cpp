#include "sx1509_keypad_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sx1509 {

static const char *TAG = "sx1509_keypad_sensor";

void SX1509KeypadSensor::setup() {
  ESP_LOGD(TAG, "setup   rows: %d , cols: %d", this->rows_, this->cols_);
  this->parent_->setup_keypad(this->rows_, this->cols_, this->sleep_time_, this->scan_time_, this->debounce_time_);
}

void SX1509KeypadSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "SX1509 Keypad:");
  ESP_LOGCONFIG(TAG, "  rows: %d , cols: %d", this->rows_, this->cols_);
}

void SX1509KeypadSensor::loop() {
  uint16_t key_data = this->parent_->read_key_data();
  if (key_data != 0) {
    uint8_t row = get_row_(key_data);
    uint8_t col = get_col_(key_data);
    uint8_t key = (row * cols_) + col;
    if (key_data != last_key_press_) {
      ESP_LOGD(TAG, "'%s' - Publishing %d  rawdata: %04x", this->name_.c_str(), key, key_data);
      this->publish_state(key);
    }
  } else if (this->last_key_press_ != 0ULL) {
    ESP_LOGD(TAG, "'%s' - publishing NAN", this->name_.c_str());
    this->publish_state(NAN);
  }
  this->last_key_press_ = key_data;

  for (auto *binary_sensor : this->binary_sensors_)
    binary_sensor->process(key_data);
}

uint8_t SX1509KeypadSensor::get_row_(uint16_t key_data) {
  uint8_t row_data = uint8_t(key_data & 0x00FF);
  for (uint16_t i = 0; i < 8; i++) {
    if (row_data & (1 << i))
      return i;
  }
  return 0;
}

uint8_t SX1509KeypadSensor::get_col_(uint16_t key_data) {
  uint8_t col_data = uint8_t((key_data & 0xFF00) >> 8);
  for (uint16_t i = 0; i < 8; i++) {
    if (col_data & (1 << i))
      return i;
  }
  return 0;
}

}  // namespace sx1509
}  // namespace esphome