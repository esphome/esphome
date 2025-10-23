#pragma once

#include "epaper_spi.h"

namespace esphome::epaper_spi {

class EPaperSpectraE6 : public EPaperBase {
 public:
  EPaperSpectraE6(const uint8_t *init_sequence, const size_t init_sequence_length)
      : EPaperBase(init_sequence, init_sequence_length) {}

  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_COLOR; }
  void fill(Color color) override;

 protected:
  void draw_absolute_pixel_internal(int x, int y, Color color) override;
  uint32_t get_buffer_length() override;

  bool transfer_data() override;
  void reset() override;
};

}  // namespace esphome::epaper_spi
