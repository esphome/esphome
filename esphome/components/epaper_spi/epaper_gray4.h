#pragma once

#include "colorconv.h"
#include "epaper_spi.h"

namespace esphome::epaper_spi {

/**
 * Combine two buffer bytes (4 pixels each, 2 bits per pixel, most significant pixel first) into one
 * plane byte covering the same 8 pixels (1 bit per pixel), taking the high or the low bit of each level.
 */
inline uint8_t gray4_plane_byte(uint8_t first, uint8_t second, bool high_bit) {
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

/**
 * Four-level grayscale framebuffer for a driver class: 2 bits per pixel, 4 pixels per byte, most
 * significant pixel first, level 0 = black to 3 = white. The driver must be constructed with
 * DISPLAY_TYPE_GRAYSCALE and 2 bits per pixel.
 */
template<typename Base> class EPaperGray4 : public Base {
 public:
  using Base::Base;

  void fill(Color color) override {
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

  void HOT draw_pixel_at(int x, int y, Color color) override {
    if (!this->rotate_coordinates_(x, y))
      return;
    const uint8_t level = color_to_gray4(color);
    const size_t byte_position = (size_t) y * this->row_width_ + x / 4;
    const uint8_t shift = (uint8_t) (6 - 2 * (x % 4));  // most significant pixel first
    const uint8_t original = this->buffer_[byte_position];
    this->buffer_[byte_position] = (uint8_t) ((original & ~(0x03 << shift)) | (level << shift));
  }
};

}  // namespace esphome::epaper_spi
