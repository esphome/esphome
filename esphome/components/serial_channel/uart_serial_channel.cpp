#include "uart_serial_channel.h"
#include "esphome/core/log.h"

namespace esphome::serial_channel {

static const char *const TAG = "serial_channel.uart";

void UARTSerialChannel::loop() {
  uint8_t byte;
  while (this->available()) {
    if (this->read_byte(&byte)) {
      this->rx_buffer_.push_back(byte);

      // Publish on buffer full, or no more data available
      if (this->rx_buffer_.size() == this->rx_buffer_.capacity_ || !this->available()) {
        break;
      }
    } else {
      // unexpected read failure, will be logged in read_byte
      break;
    }
    if (!this->rx_buffer_.empty()) {
      this->publish_state(this->rx_buffer_.data(), this->rx_buffer_.size());
      this->rx_buffer_.clear();
    }
  }
}

void UARTSerialChannel::dump_config() {
  ESP_LOGCONFIG(TAG, "UART Serial Channel '%s'", this->get_name().c_str());
  LOG_SERIAL_CHANNEL("  ", "UART Serial Channel", this);
  ESP_LOGCONFIG(TAG, "  Buffer Size: %d", this->buffer_size_);
}

void UARTSerialChannel::control(const uint8_t *data, size_t len) {
  // Write data to UART
  this->write_array(data, len);
  this->flush();
  ESP_LOGD(TAG, "'%s': Sent %d bytes to UART", this->get_name().c_str(), len);
}

}  // namespace esphome::serial_channel
