#pragma once

#include "epaper_spi_mono.h"

namespace esphome::epaper_spi {
/**
 * A class for Solomon SSD1683 epaper displays.
 */
class EPaperSSD1683 : public EPaperMono {
 public:
  EPaperSSD1683(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                size_t init_sequence_length)
      : EPaperMono(name, width, height, init_sequence, init_sequence_length) {}

 protected:
  void refresh_screen(bool partial) override;
  void deep_sleep() override;
  void set_window() override;
  bool transfer_data() override;
};

}  // namespace esphome::epaper_spi
