#include "uart_binary_sensor.h"
#include "esphome/core/log.h"

namespace esphome::uart {

static const char *const TAG = "uart.binary_sensor";

void UARTBinarySensor::setup() {}

void UARTBinarySensor::loop() {
  for (auto &matcher : this->matchers_) {
    if (matcher.triggered) {
      matcher.sensor->publish_state(false);
      matcher.triggered = false;
    }
  }
  this->read_data_();
}

void UARTBinarySensor::dump_config() { ESP_LOGCONFIG(TAG, "UART Binary Sensor:"); }

void UARTBinarySensor::add_event_matcher(binary_sensor::BinarySensor *sensor, const uint8_t *data, size_t data_len) {
  this->matchers_.push_back({sensor, data, data_len, false});
}

void UARTBinarySensor::read_data_() {
  while (this->available()) {
    uint8_t byte;
    this->read_byte(&byte);
    this->buffer_.push_overwrite(byte);

    for (auto &matcher : this->matchers_) {
      if (this->buffer_.size() < matcher.data_len) {
        continue;
      }

      size_t start = this->buffer_.size() - matcher.data_len;

      bool match = true;
      for (size_t i = 0; i < matcher.data_len; ++i) {
        if (this->buffer_[start + i] != matcher.data[i]) {
          match = false;
          break;
        }
      }

      if (match) {
        matcher.sensor->publish_state(true);
        matcher.triggered = true;
        this->buffer_.clear();
        break;
      }
    }
  }
}

}  // namespace esphome::uart
