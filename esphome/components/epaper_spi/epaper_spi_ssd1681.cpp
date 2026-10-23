#include "epaper_spi_ssd1681.h"

#include <algorithm>
#include <cstring>

#include "esphome/core/log.h"

namespace esphome::epaper_spi {
ESPHOME_LOG_TAG(TAG, "epaper_spi.ssd1681");

void EPaperSSD1681::bounds_from_changes_() {
  if (!this->sent_valid_ || this->sent_.size() != this->buffer_.size())
    return;
  bool changed = false;
  uint16_t row_low = this->height_, row_high = 0, col_low = this->row_width_, col_high = 0;
  for (uint16_t row = 0; row != this->height_; row++) {
    const size_t base = row * this->row_width_;
    const size_t end = base + this->row_width_;
    size_t index = base;
    while (index != end) {
      size_t new_length, old_length;
      const uint8_t *new_bytes = this->buffer_.get_span(index, new_length);
      const uint8_t *old_bytes = this->sent_.get_span(index, old_length);
      const size_t length = std::min({new_length, old_length, end - index});
      if (std::memcmp(new_bytes, old_bytes, length) != 0) {
        for (size_t i = 0; i != length; i++) {
          if (new_bytes[i] == old_bytes[i])
            continue;
          const auto col = static_cast<uint16_t>(index + i - base);
          changed = true;
          row_low = std::min(row_low, row);
          row_high = std::max<uint16_t>(row_high, row + 1);
          col_low = std::min(col_low, col);
          col_high = std::max<uint16_t>(col_high, col + 1);
        }
      }
      index += length;
    }
  }
  if (!changed)
    return;
  this->x_low_ = col_low * 8;
  this->x_high_ = std::min<uint16_t>(col_high * 8, this->width_);
  this->y_low_ = row_low;
  this->y_high_ = row_high;
}

bool HOT EPaperSSD1681::transfer_data() {
  if (this->current_data_index_ == 0 && this->send_red_) {
    if (!this->partial_) {
      // A refresh drives the whole panel from RAM, so a full one makes both RAM banks whole again
      this->x_low_ = 0;
      this->x_high_ = this->width_;
      this->y_low_ = 0;
      this->y_high_ = this->height_;
    } else {
      // 0x26 already holds the image on the panel: the controller copies 0x24 into it after each refresh
      this->bounds_from_changes_();
      this->set_window();
      this->send_red_ = false;
    }
  }
  return EPaperSSD1683::transfer_data();
}

void EPaperSSD1681::refresh_screen(bool partial) {
  ESP_LOGV(TAG, "Refresh screen");
  this->cmd_data(0x3C, {partial ? (uint8_t) 0x80 : (uint8_t) 0x01});
  // 0x26 is compared as it is on a partial refresh and ignored on a full one
  this->cmd_data(0x21, {partial ? (uint8_t) 0x00 : (uint8_t) 0x40, (uint8_t) 0x00});
  // Both end with analog and OSC off before the panel goes to sleep
  this->cmd_data(0x22, {partial ? (uint8_t) 0xFF : (uint8_t) 0xF7});
  this->command(0x20);
}

}  // namespace esphome::epaper_spi
