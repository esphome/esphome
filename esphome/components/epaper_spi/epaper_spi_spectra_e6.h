#pragma once

#include "epaper_spi.h"

namespace esphome::epaper_spi {

class EPaperSpectraE6 : public EPaperBase {
 public:
  EPaperSpectraE6(uint16_t width, uint16_t height, const uint8_t *init_sequence, const size_t init_sequence_length)
      : EPaperBase(width, height, init_sequence, init_sequence_length, DISPLAY_TYPE_COLOR) {}

  void fill(Color color) override;

  void dump_config() override;

 protected:
  void refresh_screen() override;
  void power_on() override;
  void power_off() override;
  void deep_sleep() override;
  void draw_absolute_pixel_internal(int x, int y, Color color) override;
  uint32_t get_buffer_length() override;

  bool transfer_data() override;
  void reset() override;
};

}  // namespace esphome::epaper_spi
