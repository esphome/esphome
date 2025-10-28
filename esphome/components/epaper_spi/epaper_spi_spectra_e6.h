#pragma once

#include "epaper_spi.h"

namespace esphome::epaper_spi {

class EPaperSpectraE6 : public EPaperBase {
 public:
  EPaperSpectraE6(uint16_t width, uint16_t height, std::vector<uint8_t> init_sequence)
      : EPaperBase(width, height, init_sequence, DISPLAY_TYPE_COLOR) {
    this->buffer_length_ = width * height / 2;  // 2 pixels per byte
  }

  void fill(Color color) override;

  void dump_config() override;

 protected:
  void refresh_screen() override;
  void power_on() override;
  void power_off() override;
  void deep_sleep() override;
  void draw_absolute_pixel_internal(int x, int y, Color color) override;

  bool transfer_data() override;
  void reset() override;
};

}  // namespace esphome::epaper_spi
