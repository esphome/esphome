#pragma once

#include "epaper_spi.h"

namespace esphome::epaper_spi {

/**
 * Intermediate base for black/white/red panels with two 1-bit-per-pixel planes (8 pixels per byte).
 *
 * Owns buffer sizing, fill()/draw_pixel_at() and the chunked SPI transfer of both planes:
 * - Buffer first half: Black/White plane (1=black, 0=white or red), sent inverted with 0x10
 * - Buffer second half: Red plane (1=red, 0=no red), sent with 0x13; inverted when invert_red is set
 * Concrete subclasses supply only their IC-specific power/refresh/sleep command sequences.
 */
class EPaperBWR : public EPaperBase {
 public:
  EPaperBWR(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
            size_t init_sequence_length, bool invert_red)
      : EPaperBase(name, width, height, init_sequence, init_sequence_length, DISPLAY_TYPE_COLOR),
        invert_red_(invert_red) {
    this->buffer_length_ = this->row_width_ * height * 2;
  }

  void fill(Color color) override;

 protected:
  void draw_pixel_at(int x, int y, Color color) override;
  bool transfer_data() override;

  // Streams buffer_[current_data_index_ .. end) XORed with invert_mask in chunks, with chip select toggled per
  // chunk; returns false if it yields on MAX_TRANSFER_TIME.
  bool send_buffer_range_(size_t end, uint8_t invert_mask, uint32_t start_time);

  // Send the red plane as 0=red, 1=no red
  bool invert_red_;
};

}  // namespace esphome::epaper_spi
