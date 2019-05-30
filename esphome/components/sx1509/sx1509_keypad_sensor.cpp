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

}  // namespace sx1509
}  // namespace esphome