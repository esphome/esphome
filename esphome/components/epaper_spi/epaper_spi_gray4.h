#pragma once

#include "colorconv.h"
#include "epaper_spi.h"

namespace esphome::epaper_spi {

// Combine two source bytes (4 pixels each, 2 bits per pixel, most significant pixel first) into
// one plane byte covering the same 8 pixels (1 bit per pixel), choosing high or low bit
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
 * Adds a framebuffer of four gray levels to a driver: 2 bits per pixel, 4 per byte, most significant
 * pixel first, with level 0 black and 3 white. Base is the driver class for the panel's controller.
 */
template<typename Base> class EPaperGray4 : public Base {
 public:
  EPaperGray4(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
              size_t init_sequence_length)
      : Base(name, width, height, init_sequence, init_sequence_length, DISPLAY_TYPE_GRAYSCALE, 2) {}

  void fill(Color color) override {
    if (this->get_clipping().is_set()) {
      // Falls back to the generic per-pixel implementation for the clipped rectangle.
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
    const size_t byte_position = (size_t) y * this->row_width_ + x / 4;
    const uint8_t shift = (uint8_t) (6 - 2 * (x % 4));
    const uint8_t original = this->buffer_[byte_position];
    this->buffer_[byte_position] = (uint8_t) ((original & ~(0x03 << shift)) | (level << shift));
  }

 protected:
  // A partial update reduces each pixel to its high bit: levels 2 and 3 are light, 0 and 1 dark.
  void mono_row(size_t y, uint8_t *out) override {
    const size_t src_row = y * this->row_width_;
    for (size_t i = 0; i != this->mono_row_length_(); i++)
      out[i] = gray4_plane_byte(this->buffer_[src_row + 2 * i], this->buffer_[src_row + 2 * i + 1], true);
  }
};

}  // namespace esphome::epaper_spi
