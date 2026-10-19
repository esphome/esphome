#pragma once

#include "epaper_spi_bwr.h"

namespace esphome::epaper_spi {

/**
 * WeAct 3-color e-paper displays (SSD1683 controller).
 * Supports multiple sizes: 2.9" (128x296), 4.2" (400x300), etc.
 *
 * Buffer layout: see EPaperBWR, with red_sets_black set so red pixels are sent as 0 in the black/white plane.
 * The planes are sent in the opposite order: red with 0x26, then black/white with 0x24.
 */
class EPaperWeAct3C : public EPaperBWR {
 public:
  EPaperWeAct3C(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                size_t init_sequence_length, bool invert_red = false)
      : EPaperBWR(name, width, height, init_sequence, init_sequence_length, invert_red, /*red_sets_black=*/true) {}

 protected:
  void set_window_();
  void refresh_screen(bool partial) override;
  void power_on() override;
  void power_off() override;
  void deep_sleep() override;

  bool transfer_data() override;
};

}  // namespace esphome::epaper_spi
