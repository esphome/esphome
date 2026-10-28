#include "epaper_spi_uc8179_gray4.h"

#include "esphome/core/log.h"

namespace esphome::epaper_spi {

ESPHOME_LOG_TAG(TAG, "epaper_spi.uc8179_gray4");

// Combine two source bytes (4 pixels each, 2 bits per pixel, most significant pixel first) into
// one plane byte covering the same 8 pixels (1 bit per pixel), choosing high or low bit
static uint8_t plane_byte(uint8_t first, uint8_t second, bool high_bit) {
  uint8_t out = 0;
  for (const uint8_t src : {first, second}) {
    for (uint8_t shift = 6;; shift -= 2) {
      const uint8_t level = (src >> shift) & 0x03;
      out = (uint8_t) ((out << 1) | (high_bit ? (level >> 1) : (level & 1)));
      if (shift == 0)
        break;
    }
  }
  return out;
}

void EPaperUC8179Gray4::fill(Color color) {
  if (this->get_clipping().is_set()) {
    // Falls back to the generic per-pixel implementation for the clipped rectangle.
    EPaperBase::fill(color);
    return;
  }
  const uint8_t level = color_to_gray4(color);
  this->buffer_.fill((uint8_t) (level | (level << 2) | (level << 4) | (level << 6)));
  this->mark_all_drawn_();
}

void HOT EPaperUC8179Gray4::draw_pixel_at(int x, int y, Color color) {
  if (!this->rotate_coordinates_(x, y))
    return;
  const uint8_t level = color_to_gray4(color);
  const size_t byte_position = (size_t) y * this->row_width_ + x / 4;
  const uint8_t shift = (uint8_t) (6 - 2 * (x % 4));  // most significant pixel first
  const uint8_t original = this->buffer_[byte_position];
  this->buffer_[byte_position] = (uint8_t) ((original & ~(0x03 << shift)) | (level << shift));
}

// current_data_index_ counts rows across both planes: the low bit of every pixel's level goes to
// DTM1 (0x10) first, then the high bit to DTM2 (0x13). Both are sent inverted, 0 = white.
bool HOT EPaperUC8179Gray4::transfer_data() {
  const uint32_t start_time = millis();
  const size_t height = this->height_;
  if (this->current_data_index_ == 0) {
    ESP_LOGV(TAG, "Selecting the four-level waveform");
    // These registers are only accepted while powered, so they follow power-on
    this->cmd_data(CMD_BOOSTER_SOFT_START, {0x27, 0x27, 0x18, 0x17});
    this->cmd_data(CMD_CCSET, {0x02});
    this->cmd_data(CMD_TSSET, {TSSET_GRAY4});
  }
  const size_t plane_row_length = this->width_ / 8;
  // Stack-backed for every panel width in practice; only a custom `dimensions:` far wider than any
  // supported panel would fall back to the heap.
  SmallBufferWithHeapFallback<128> bytes_to_send_alloc(plane_row_length);
  uint8_t *bytes_to_send = bytes_to_send_alloc.get();

  while (this->current_data_index_ != 2 * height) {
    const bool high_bit = this->current_data_index_ >= height;
    if (this->current_data_index_ == 0 || this->current_data_index_ == height)
      this->command(high_bit ? CMD_DTM2 : CMD_DTM1);
    const size_t plane_end = high_bit ? 2 * height : height;
    this->start_data_();
    while (this->current_data_index_ != plane_end) {
      const size_t y = high_bit ? this->current_data_index_ - height : this->current_data_index_;
      const size_t src_row = y * this->row_width_;
      for (size_t i = 0; i != plane_row_length; i++) {
        bytes_to_send[i] =
            (uint8_t) ~plane_byte(this->buffer_[src_row + 2 * i], this->buffer_[src_row + 2 * i + 1], high_bit);
      }
      this->write_array(bytes_to_send, plane_row_length);
      ++this->current_data_index_;
      if (millis() - start_time > MAX_TRANSFER_TIME) {
        // Let the main loop run and come back next loop
        this->disable();
        return false;
      }
    }
    this->disable();
  }

  this->current_data_index_ = 0;
  return true;
}

}  // namespace esphome::epaper_spi
