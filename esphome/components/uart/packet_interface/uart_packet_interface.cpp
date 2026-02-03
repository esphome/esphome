#include "esphome/core/log.h"
#include "uart_packet_interface.h"

namespace esphome {
namespace uart {

static const char *const TAG = "uart.packet_interface";

void UartPacketInterface::loop() {
  while (this->available()) {
    uint8_t byte;
    if (!this->read_byte(&byte)) {
      ESP_LOGW(TAG, "Failed to read byte from UART");
      return;
    }
    if (byte == FLAG_BYTE) {
      if (this->rx_started_ && !this->receive_buffer_.empty()) {
        PacketBuffer buffer(this->receive_buffer_);
        this->on_receive_from_interface_(buffer, {});
        this->rx_started_ = false;
      } else {
        this->rx_started_ = true;
      }
      this->receive_buffer_.clear();
      this->rx_control_ = false;
      continue;
    }
    if (!this->rx_started_)
      continue;
    if (byte == CONTROL_BYTE) {
      this->rx_control_ = true;
      continue;
    }
    if (this->rx_control_) {
      byte ^= 0x20;
      this->rx_control_ = false;
    }
    if (this->receive_buffer_.size() >= this->rx_buffer_size_) {
      ESP_LOGD(TAG, "Packet too large, discarding");
      this->rx_started_ = false;
      this->receive_buffer_.clear();
      continue;
    }
    this->receive_buffer_.push_back(byte);
  }
}

/**
 * Write a byte to the UART bus. If the byte is a flag or control byte, it will be escaped.
 * @param byte The byte to write.
 */
void UartPacketInterface::write_byte_(uint8_t byte) {
  if (byte == FLAG_BYTE || byte == CONTROL_BYTE) {
    this->write_byte(CONTROL_BYTE);
    byte ^= 0x20;
  }
  this->write_byte(byte);
}

bool UartPacketInterface::send_to_interface(const PacketBuffer &data, PacketMetaData meta_data) {
  size_t size = data.size();
  if (size == 0) {
    return true;
  }

  // Write frame start
  this->write_byte(FLAG_BYTE);

  // Write data with byte stuffing
  for (size_t i = 0; i < size; i++) {
    this->write_byte_(data[i]);
  }

  // Write frame end
  this->write_byte(FLAG_BYTE);

  return true;
}

}  // namespace uart
}  // namespace esphome
