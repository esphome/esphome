#pragma once

#include "epaper_spi.h"

namespace esphome::epaper_spi {

class EPaperInkplate2 : public EPaperBase {
 public:
  EPaperInkplate2(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                  size_t init_sequence_length)
      : EPaperBase(name, width, height, init_sequence, init_sequence_length, DISPLAY_TYPE_COLOR) {
    // Dual buffer: B/W buffer + Red buffer (1 bit per pixel for each)
    this->buffer_length_ = (width * height / 8) * 2;
  }

  void fill(Color color) override;
  void clear() override;

 protected:
  void refresh_screen() override;
  void power_on() override;
  void power_off() override;
  void deep_sleep() override;
  void draw_absolute_pixel_internal(int x, int y, Color color) override;

  bool transfer_data() override;
};

}  // namespace esphome::epaper_spi
