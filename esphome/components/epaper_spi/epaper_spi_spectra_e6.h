#pragma once

#include "epaper_spi.h"

namespace esphome::epaper_spi {

class EPaperSpectraE6 : public EPaperBase {
 public:
  EPaperSpectraE6(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                  size_t init_sequence_length)
      : EPaperBase(name, width, height, init_sequence, init_sequence_length, DISPLAY_TYPE_COLOR) {
    this->buffer_length_ = width * height / 2;  // 2 pixels per byte
  }

  void fill(Color color) override;
  void clear() override;

 protected:
  void refresh_screen(bool partial) override;
  void power_on() override;
  void power_off() override;
  void deep_sleep() override;
  void draw_pixel_at(int x, int y, Color color) override;

  bool transfer_data() override;
};

}  // namespace esphome::epaper_spi
