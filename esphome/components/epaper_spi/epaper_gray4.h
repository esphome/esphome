#pragma once

#include "colorconv.h"
#include "epaper_spi.h"

namespace esphome::epaper_spi {

/// The high (shift 1) or low (shift 0) bit of each of a byte's four 2-bit levels, as a nibble, first pixel highest.
inline uint8_t gray4_nibble(uint8_t src, uint8_t shift) {
  uint8_t v = (src >> shift) & 0x55;
  v = (v | (v >> 1)) & 0x33;
  return (v | (v >> 2)) & 0x0F;
}

/**
 * Combine two buffer bytes (4 pixels each, 2 bits per pixel, most significant pixel first) into one
 * plane byte covering the same 8 pixels (1 bit per pixel), taking the high or the low bit of each level.
 */
inline uint8_t gray4_plane_byte(uint8_t first, uint8_t second, bool high_bit) {
  const uint8_t shift = high_bit ? 1 : 0;
  return (uint8_t) ((gray4_nibble(first, shift) << 4) | gray4_nibble(second, shift));
}

/**
 * Four-level grayscale framebuffer for a driver class: 2 bits per pixel, 4 pixels per byte, most
 * significant pixel first, level 0 = black to 3 = white.
 */
template<typename Base> class EPaperGray4 : public Base {
 public:
  EPaperGray4(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
              size_t init_sequence_length)
      : Base(name, width, height, init_sequence, init_sequence_length, DISPLAY_TYPE_GRAYSCALE, 2) {}

  void fill(Color color) override {
    if (this->get_clipping().is_set()) {
      // The base's clipped path draws pixel by pixel through this class's draw_pixel_at()
      Base::fill(color);
      return;
    }
    const uint8_t level = color_to_gray4(color);
    this->buffer_.fill((uint8_t) (level | (level << 2) | (level << 4) | (level << 6)));
    this->mark_all_drawn_();
  }

  void HOT draw_pixel_at(int x, int y, Color color) override {
    if (!this->rotate_coordinates_(x, y))
      return;
    const uint8_t level = color_to_gray4(color);
    const uint8_t shift = (uint8_t) (6 - 2 * (x % 4));  // most significant pixel first
    uint8_t &byte = this->buffer_[(size_t) y * this->row_width_ + x / 4];
    byte = (uint8_t) ((byte & ~(0x03 << shift)) | (level << shift));
  }
};

}  // namespace esphome::epaper_spi
