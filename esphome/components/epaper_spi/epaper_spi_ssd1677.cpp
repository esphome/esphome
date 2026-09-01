#include "epaper_spi_ssd1677.h"

#include <algorithm>

#include "esphome/core/log.h"

namespace esphome::epaper_spi {
static constexpr const char *const TAG = "epaper_spi.ssd1677";

// Which RAM bits (bw = command 0x24, red = command 0x26) select each gray level's waveform slot,
// for the panel's factory OTP gray4 path. Sourced from Seeed's own `seeed_epaper` SSD1677 driver and
// cross-checked against the stock reTerminal Sticky firmware image; not derived from GxEPD2.
static uint8_t gray_to_bw_bit(uint8_t gray) { return gray <= 1; }
static uint8_t gray_to_red_bit(uint8_t gray) { return gray == 0 || gray == 2; }

uint8_t EPaperSSD1677::color_to_gray_(Color color) {
  // Extends EPaperBase::color_to_bit()'s half-way luminance split into four even bands.
  const uint16_t sum = color.r + color.g + color.b;
  if (sum >= 573)
    return 3;  // white
  if (sum >= 382)
    return 2;  // light gray
  if (sum >= 191)
    return 1;  // dark gray
  return 0;    // black
}

void HOT EPaperSSD1677::draw_pixel_at(int x, int y, Color color) {
  if (!this->grayscale_) {
    EPaperBase::draw_pixel_at(x, y, color);
    return;
  }
  if (!rotate_coordinates_(x, y))
    return;
  const uint8_t gray = color_to_gray_(color);
  const size_t byte_position = y * this->row_width_ + x / 8;
  const uint8_t pixel_bit = 0x80 >> (x % 8);
  const size_t plane_bytes = this->row_width_ * this->height_;

  const auto bw_byte = this->buffer_[byte_position];
  this->buffer_[byte_position] = gray_to_bw_bit(gray) ? (bw_byte | pixel_bit) : (bw_byte & ~pixel_bit);
  const auto red_byte = this->buffer_[plane_bytes + byte_position];
  this->buffer_[plane_bytes + byte_position] = gray_to_red_bit(gray) ? (red_byte | pixel_bit) : (red_byte & ~pixel_bit);
}

void EPaperSSD1677::fill(Color color) {
  if (!this->grayscale_) {
    EPaperBase::fill(color);
    return;
  }
  if (this->get_clipping().is_set()) {
    Display::fill(color);
    return;
  }
  const uint8_t gray = color_to_gray_(color);
  const bool bw_bit = gray_to_bw_bit(gray);
  const bool red_bit = gray_to_red_bit(gray);
  const size_t plane_bytes = this->row_width_ * this->height_;
  if (bw_bit == red_bit) {
    // Both planes take the same byte pattern (true for black and white); one bulk fill covers both.
    this->buffer_.fill(bw_bit ? 0xFF : 0x00);
  } else {
    const uint8_t bw_byte = bw_bit ? 0xFF : 0x00;
    const uint8_t red_byte = red_bit ? 0xFF : 0x00;
    for (size_t i = 0; i != plane_bytes; i++) {
      this->buffer_[i] = bw_byte;
      this->buffer_[plane_bytes + i] = red_byte;
    }
  }
  this->x_high_ = this->width_;
  this->y_high_ = this->height_;
  this->x_low_ = 0;
  this->y_low_ = 0;
}

void EPaperSSD1677::refresh_screen(bool partial) {
  if (!this->grayscale_) {
    EPaperMono::refresh_screen(partial);
    return;
  }
  ESP_LOGV(TAG, "Refresh screen (gray4, OTP waveform)");
  this->cmd_data(0x3C, {0x00});        // border waveform: follow LUT0 (Seeed's gray4 value)
  this->cmd_data(0x1A, {0x67, 0x00});  // force the panel's stored gray4 waveform via its OTP temperature trick
  this->cmd_data(0x22, {0xD7});        // Seeed/stock gray4 update sequence (not a named row in the datasheet excerpt)
  this->command(0x20);
}

void EPaperSSD1677::set_window() {
  if (!this->grayscale_) {
    EPaperMono::set_window();
    return;
  }
  // Grayscale always redraws the full frame; there is no partial-refresh path here (see class comment).
  this->x_low_ = 0;
  this->x_high_ = this->width_;
  this->y_low_ = 0;
  this->y_high_ = this->height_;
  this->cmd_data(0x44, {(uint8_t) this->x_low_, (uint8_t) (this->x_low_ / 256), (uint8_t) (this->x_high_ - 1),
                        (uint8_t) ((this->x_high_ - 1) / 256)});
  this->cmd_data(0x4E, {(uint8_t) this->x_low_, (uint8_t) (this->x_low_ / 256)});
  this->cmd_data(0x45, {(uint8_t) this->y_low_, (uint8_t) (this->y_low_ / 256), (uint8_t) (this->y_high_ - 1),
                        (uint8_t) ((this->y_high_ - 1) / 256)});
  this->cmd_data(0x4F, {(uint8_t) this->y_low_, (uint8_t) (this->y_low_ / 256)});
}

bool HOT EPaperSSD1677::transfer_data() {
  if (!this->grayscale_) {
    return EPaperMono::transfer_data();
  }
  auto start_time = millis();
  const size_t plane_bytes = this->row_width_ * this->height_;
  if (this->current_data_index_ == 0) {
    if (this->send_red_) {
      this->set_window();
    }
    this->command(this->send_red_ ? 0x26 : 0x24);
    this->current_data_index_ = this->y_low_;  // actually current line
  }
  const size_t row_length = (this->x_high_ - this->x_low_) / 8;
  const size_t plane_offset = this->send_red_ ? plane_bytes : 0;
  FixedVector<uint8_t> bytes_to_send{};
  bytes_to_send.init(row_length);
  ESP_LOGV(TAG, "Writing %u bytes at line %zu at %ums", row_length, this->current_data_index_, (unsigned) millis());
  this->start_data_();
  while (this->current_data_index_ != this->y_high_) {
    size_t data_idx = plane_offset + this->current_data_index_ * this->row_width_ + this->x_low_ / 8;
    for (size_t i = 0; i != row_length; i++) {
      bytes_to_send[i] = this->buffer_[data_idx++];
    }
    ++this->current_data_index_;
    this->write_array(&bytes_to_send.front(), row_length);  // NOLINT
    if (millis() - start_time > MAX_TRANSFER_TIME) {
      // Let the main loop run and come back next loop
      this->disable();
      return false;
    }
  }

  this->disable();
  this->current_data_index_ = 0;
  if (this->send_red_) {
    this->send_red_ = false;
    return false;
  }
  this->send_red_ = true;
  return true;
}

void EPaperSSD1677::deep_sleep() {
  if (!this->grayscale_) {
    EPaperMono::deep_sleep();
    return;
  }
  // Every gray4 refresh redraws from this->buffer_ rather than relying on retained panel RAM, so
  // there is no reason to skip sleep the way the partial-update-aware base class does.
  ESP_LOGV(TAG, "Deep sleep");
  this->cmd_data(0x10, {0x03});
}

}  // namespace esphome::epaper_spi
