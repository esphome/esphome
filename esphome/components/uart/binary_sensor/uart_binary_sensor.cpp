#include "uart_binary_sensor.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::uart {

ESPHOME_LOG_TAG(TAG, "uart.binary_sensor");

void UARTBinarySensorHub::dump_config() {
  ESP_LOGCONFIG(TAG, "UART Binary Sensor:");
  for (const auto &matcher : this->matchers_) {
    LOG_BINARY_SENSOR("  ", "Pattern", matcher.sensor);
  }
}

void UARTBinarySensorHub::loop() {
  this->read_available([this](uint8_t byte) {
    this->buffer_.push_overwrite(byte);
    for (const auto &matcher : this->matchers_) {
      // The byte just read has to be the pattern's last one before the rest is compared
      if (progmem_read_byte(&matcher.data[matcher.data_len - 1]) != byte || !this->matches_(matcher)) {
        continue;
      }
      // A momentary press, as the remote receiver binary sensors publish theirs
      matcher.sensor->publish_state(true);
      matcher.sensor->publish_state(false);
      this->buffer_.clear();
      break;
    }
  });
}

// Whether the pattern, apart from its last byte, ends the buffer
bool UARTBinarySensorHub::matches_(const EventMatcher &matcher) const {
  if (this->buffer_.size() < matcher.data_len) {
    return false;
  }
  const size_t start = this->buffer_.size() - matcher.data_len;
  for (size_t i = 0; i + 1 < matcher.data_len; i++) {
    if (this->buffer_[start + i] != progmem_read_byte(&matcher.data[i])) {
      return false;
    }
  }
  return true;
}

}  // namespace esphome::uart
