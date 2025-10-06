#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "uart_transport.h"

namespace esphome {
namespace uart {

static const char *const TAG = "uart_transport";

void UARTTransport::loop() {
  PacketTransport::loop();

  while (this->parent_->available()) {
    uint8_t byte;
    if (!this->parent_->read_byte(&byte)) {
      ESP_LOGW(TAG, "Failed to read byte from UART");
      return;
    }
    if (byte == FLAG_BYTE) {
      if (this->rx_started_ && this->receive_buffer_.size() > 6) {
        auto len = this->receive_buffer_.size();
        auto crc = crc16(this->receive_buffer_.data(), len - 2);
        if (crc != (this->receive_buffer_[len - 2] | (this->receive_buffer_[len - 1] << 8))) {
          ESP_LOGD(TAG, "CRC mismatch, discarding packet");
          this->rx_started_ = false;
          this->receive_buffer_.clear();
          continue;
        }
        this->receive_buffer_.resize(len - 2);
        this->process_(this->receive_buffer_);
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
    if (this->receive_buffer_.size() == MAX_PACKET_SIZE) {
      ESP_LOGD(TAG, "Packet too large, discarding");
      this->rx_started_ = false;
      this->receive_buffer_.clear();
      continue;
    }
    this->receive_buffer_.push_back(byte);
  }
}

void UARTTransport::update() {
  this->updated_ = true;
  this->resend_data_ = true;
  PacketTransport::update();
}

/**
 * Write a byte to the UART bus. If the byte is a flag or control byte, it will be escaped.
 * @param byte The byte to write.
 */
void UARTTransport::write_byte_(uint8_t byte) const {
  if (byte == FLAG_BYTE || byte == CONTROL_BYTE) {
    this->parent_->write_byte(CONTROL_BYTE);
    byte ^= 0x20;
  }
  this->parent_->write_byte(byte);
}

void UARTTransport::send_packet(const std::vector<uint8_t> &buf) const {
  this->parent_->write_byte(FLAG_BYTE);
  for (uint8_t byte : buf) {
    this->write_byte_(byte);
  }
  auto crc = crc16(buf.data(), buf.size());
  this->write_byte_(crc & 0xFF);
  this->write_byte_(crc >> 8);
  this->parent_->write_byte(FLAG_BYTE);
}
}  // namespace uart
}  // namespace esphome
