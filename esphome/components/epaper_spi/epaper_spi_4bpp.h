#pragma once

#include "epaper_spi.h"

namespace esphome::epaper_spi {

/**
 * Intermediate base for panels with a 4-bit-per-pixel native buffer layout (2 pixels per byte).
 *
 * Owns buffer sizing, fill()/clear()/draw_pixel_at() and the chunked SPI transfer loop shared by
 * this family of controllers. Concrete subclasses supply only their RGB -> 4-bit color mapping via
 * color_to_native() plus their IC-specific power/refresh/sleep command sequences.
 */
class EPaper4bpp : public EPaperBase {
 public:
  EPaper4bpp(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
             size_t init_sequence_length)
      : EPaperBase(name, width, height, init_sequence, init_sequence_length, DISPLAY_TYPE_COLOR) {
    this->buffer_length_ = width * height / 2;  // 2 pixels per byte
  }

  void fill(Color color) override;
  void clear() override;

 protected:
  void draw_pixel_at(int x, int y, Color color) override;
  bool transfer_data() override;

  /// Map an RGB color to this panel's native 4-bit color key (low nibble).
  virtual uint8_t color_to_native(Color color) = 0;

  static constexpr uint8_t CMD_TRANSFER_DATA = 0x10;
};

}  // namespace esphome::epaper_spi
