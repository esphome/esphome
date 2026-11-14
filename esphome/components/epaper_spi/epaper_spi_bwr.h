#pragma once

#include "epaper_spi.h"

namespace esphome::epaper_spi {

/**
 * Intermediate base for black/white/red panels with two 1-bit-per-pixel planes (8 pixels per byte).
 *
 * Provides fill_buffer(), draw_pixel_at() and the chunked SPI transfer of both planes:
 * - Buffer first half: Black/White plane (1=black, 0=white or red; 1=black or red when red_sets_black is set),
 *   always sent inverted
 * - Buffer second half: Red plane (1=red, 0=no red), inverted when invert_red is set
 * The default transfer_data() sends the black/white plane with 0x10, then the red plane with 0x13. Subclasses
 * supply their IC-specific power/refresh/sleep commands, and may override transfer_data() for other commands or
 * plane order, using send_buffer_range_() to send each plane.
 */
class EPaperBWR : public EPaperBase {
 public:
  EPaperBWR(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
            size_t init_sequence_length, bool invert_red, bool red_sets_black = false)
      : EPaperBase(name, width, height, init_sequence, init_sequence_length, DISPLAY_TYPE_COLOR, 1, 2),
        invert_red_(invert_red),
        red_sets_black_(red_sets_black) {}

 protected:
  void fill_buffer(Color color) override;
  void draw_pixel_at(int x, int y, Color color) override;
  bool transfer_data() override;

  // Streams buffer_[current_data_index_ .. end) XORed with invert_mask in chunks; returns false if it yields on
  // MAX_TRANSFER_TIME. Chip select is held for the whole range and released only on yield or completion.
  // It only yields with data left to send, never at end: callers rely on this to tell from current_data_index_
  // alone which plane a resumed push is in, and that a plane has just finished.
  bool send_buffer_range_(size_t end, uint8_t invert_mask, uint32_t start_time);
  // Sets buffer_[index .. end) to value, one contiguous span at a time.
  void fill_range_(size_t index, size_t end, uint8_t value);

  // Send the red plane as 0=red, 1=no red
  bool invert_red_;
  // Red pixels also set their bit in the black/white plane, for panels that need black under red
  bool red_sets_black_;
};

}  // namespace esphome::epaper_spi
