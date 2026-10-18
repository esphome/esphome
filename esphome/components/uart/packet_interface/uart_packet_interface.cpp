#include "esphome/core/log.h"
#include "uart_packet_interface.h"

namespace esphome::uart {

using packet_interface::PacketBuffer;
using packet_interface::PacketMetaData;

ESPHOME_LOG_TAG(TAG, "uart.packet_interface");

static constexpr uint8_t FLAG_BYTE = 0x7E;
static constexpr uint8_t CONTROL_BYTE = 0x7D;
static constexpr uint8_t ESCAPE_XOR = 0x20;
// Raw bytes escaped and written per UART write
static constexpr size_t TX_CHUNK_SIZE = 32;

void UartPacketInterface::loop() {
  while (this->available()) {
    uint8_t byte;
    if (!this->read_byte(&byte)) {
      ESP_LOGW(TAG, "Failed to read byte from UART");
      return;
    }
    if (byte == FLAG_BYTE) {
      // A flag that ends a frame also starts the next, so an unfinished frame cannot hide the one after it
      if (this->rx_started_ && this->rx_length_ != 0)
        this->on_receive_from_interface_(PacketBuffer(this->rx_buffer_.data(), this->rx_length_), {});
      this->rx_started_ = true;
      this->rx_length_ = 0;
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
      byte ^= ESCAPE_XOR;
      this->rx_control_ = false;
    }
    if (this->rx_length_ >= this->max_packet_size_ || this->rx_length_ >= this->rx_buffer_.size()) {
      ESP_LOGD(TAG, "Packet too large, discarding");
      this->rx_started_ = false;
      this->rx_length_ = 0;
      continue;
    }
    this->rx_buffer_[this->rx_length_++] = byte;
  }
}

bool UartPacketInterface::send_to_interface(const PacketBuffer &data, const PacketMetaData &meta_data) {
  size_t size = data.size();
  if (size == 0)
    return true;
  if (size > this->max_packet_size_) {
    ESP_LOGW(TAG, "Packet of %zu bytes is larger than %zu", size, this->max_packet_size_);
    return false;
  }
  uint8_t raw[TX_CHUNK_SIZE]{};
  // Every byte may be escaped, plus a flag byte at each end
  uint8_t out[TX_CHUNK_SIZE * 2 + 2];
  size_t out_len = 0;
  out[out_len++] = FLAG_BYTE;
  for (size_t offset = 0; offset < size;) {
    size_t count = data.copy_to(raw, TX_CHUNK_SIZE, offset);
    offset += count;
    for (size_t i = 0; i != count; i++) {
      uint8_t byte = raw[i];
      if (byte == FLAG_BYTE || byte == CONTROL_BYTE) {
        out[out_len++] = CONTROL_BYTE;
        byte ^= ESCAPE_XOR;
      }
      out[out_len++] = byte;
    }
    if (offset == size)
      out[out_len++] = FLAG_BYTE;
    this->write_array(out, out_len);
    out_len = 0;
  }
  return true;
}

}  // namespace esphome::uart
