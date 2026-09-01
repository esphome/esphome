#include "ds1603l.h"

#include <cstring>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::ds1603l {

static const char *const TAG = "ds1603l.sensor";

void DS1603L::loop() {
  // Assemble frames one byte at a time so a stream that starts mid-frame can realign
  uint8_t byte;
  while (this->available() > 0 && this->read_byte(&byte)) {
    if (this->rx_count_ == 0 && byte != HEADER_BYTE) {
      ESP_LOGV(TAG, "Skipping byte 0x%02X while looking for header", byte);
      continue;
    }

    this->rx_buffer_[this->rx_count_++] = byte;
    if (this->rx_count_ < FRAME_SIZE) {
      continue;
    }

    if (this->parse_data_()) {
      this->rx_count_ = 0;
    } else {
      // The header byte was part of the payload of a misaligned frame, so realign instead of dropping everything
      this->resync_();
    }
  }
}

void DS1603L::dump_config() { LOG_SENSOR("", "DS1603L", this); }

bool DS1603L::parse_data_() {
  uint8_t header = this->rx_buffer_[0];
  uint8_t data_h = this->rx_buffer_[1];
  uint8_t data_l = this->rx_buffer_[2];
  uint8_t checksum = this->rx_buffer_[3];

  uint8_t computed_checksum = (header + data_h + data_l) & 0xFF;

  ESP_LOGV(TAG, "Data: Header=0x%02X, Data_H=0x%02X, Data_L=0x%02X, Checksum=0x%02X", header, data_h, data_l, checksum);

  if (checksum != computed_checksum) {
    ESP_LOGW(TAG, "Checksum mismatch: received 0x%02X, expected 0x%02X", checksum, computed_checksum);
    return false;
  }

  this->publish_state(encode_uint16(data_h, data_l));
  return true;
}

void DS1603L::resync_() {
  // Drop the byte that was treated as the header, then look for the next candidate header in what is left
  size_t start = 1;
  while (start < this->rx_count_ && this->rx_buffer_[start] != HEADER_BYTE) {
    start++;
  }
  this->rx_count_ -= start;
  if (this->rx_count_ > 0) {
    memmove(this->rx_buffer_, this->rx_buffer_ + start, this->rx_count_);
  }
}

}  // namespace esphome::ds1603l
