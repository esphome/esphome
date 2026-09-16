#include "epaper_spi_ssd1677_gray4.h"

#include "esphome/core/log.h"

namespace esphome::epaper_spi {

static constexpr const char *const TAG = "epaper_spi.ssd1677_gray4";

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

void EPaperSSD1677Gray4::fill(Color color) {
  if (this->get_clipping().is_set()) {
    // Falls back to the generic per-pixel implementation for the clipped rectangle.
    EPaperBase::fill(color);
    return;
  }
  const uint8_t level = color_to_gray4(color);
  this->buffer_.fill((uint8_t) (level | (level << 2) | (level << 4) | (level << 6)));
  this->x_low_ = 0;
  this->y_low_ = 0;
  this->x_high_ = this->width_;
  this->y_high_ = this->height_;
}

void HOT EPaperSSD1677Gray4::draw_pixel_at(int x, int y, Color color) {
  if (!this->rotate_coordinates_(x, y))
    return;
  const uint8_t level = color_to_gray4(color);
  const size_t byte_position = (size_t) y * this->row_width_ + x / 4;
  const uint8_t shift = (uint8_t) (6 - 2 * (x % 4));  // most significant pixel first
  const uint8_t original = this->buffer_[byte_position];
  this->buffer_[byte_position] = (uint8_t) ((original & ~(0x03 << shift)) | (level << shift));
}

// the high bit of every pixel's level goes to the new (bw) plane
// (0x24), the low bit to the old (red) plane (0x26)
bool HOT EPaperSSD1677Gray4::transfer_data() {
  auto start_time = millis();
  const bool first_pass = this->send_red_;
  if (this->current_data_index_ == 0) {
    if (first_pass)
      this->set_window();
    this->command(first_pass ? 0x24 : 0x26);
    this->current_data_index_ = this->y_low_;
  }
  const size_t plane_row_length = (this->x_high_ - this->x_low_) / 8;
  FixedVector<uint8_t> bytes_to_send{};
  bytes_to_send.init(plane_row_length);
  ESP_LOGV(TAG, "Writing %u bytes at line %zu at %ums", plane_row_length, this->current_data_index_,
           (unsigned) millis());
  this->start_data_();
  while (this->current_data_index_ != this->y_high_) {
    const size_t src_row = this->current_data_index_ * this->row_width_ + this->x_low_ / 4;
    for (size_t i = 0; i != plane_row_length; i++) {
      const uint8_t plane = plane_byte(this->buffer_[src_row + 2 * i], this->buffer_[src_row + 2 * i + 1], first_pass);
      // The OTP grayscale waveform treats data as inverted relative to monochrome
      bytes_to_send[i] = (uint8_t) ~plane;
    }
    ++this->current_data_index_;
    this->write_array(&bytes_to_send.front(), plane_row_length);  // NOLINT
    if (millis() - start_time > MAX_TRANSFER_TIME) {
      // Let the main loop run and come back next loop
      this->disable();
      return false;
    }
  }

  this->disable();
  this->current_data_index_ = 0;
  if (first_pass) {
    this->send_red_ = false;
    return false;
  }
  this->send_red_ = true;
  return true;
}

void EPaperSSD1677Gray4::refresh_screen(bool partial) {
  // Full refresh only; the model schema rejects full_update_every other than 1 for this class.
  ESP_LOGV(TAG, "Four-level refresh");
  this->cmd_data(0x1A, {0x67, 0x00});  // force temperature by OTP
  this->cmd_data(0x22, {0xD7});        // four-level update sequence, panel's OTP waveform
  this->command(0x20);                 // master activation
}

}  // namespace esphome::epaper_spi
