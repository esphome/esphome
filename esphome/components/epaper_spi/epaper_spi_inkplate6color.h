#pragma once

#include "epaper_spi.h"

namespace esphome::epaper_spi {

// Soldered Inkplate 6COLOR: 600x448 7-color (black/white/green/blue/red/yellow/orange) e-paper,
// UC8159-family controller.
class EPaperInkplate6Color final : public EPaperBase {
 public:
  EPaperInkplate6Color(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                       size_t init_sequence_length)
      : EPaperBase(name, width, height, init_sequence, init_sequence_length, DISPLAY_TYPE_COLOR) {
    this->buffer_length_ = width * height / 2;  // 2 pixels per byte
  }

  void fill(Color color) override;
  void clear() override;
  void draw_pixel_at(int x, int y, Color color) override;

 protected:
  void refresh_screen(bool partial) override;
  void power_on() override;
  void power_off() override;
  void deep_sleep() override;

  bool transfer_data() override;
};

}  // namespace esphome::epaper_spi
