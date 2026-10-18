#pragma once

#include "epaper_gray4.h"
#include "epaper_spi_ultrachip.h"

namespace esphome::epaper_spi {

/**
 * Four-level grayscale for the UC8179 7.5" V2 panel (800x480), driven by LUTs uploaded with the
 * model's init sequence. Full refresh only.
 *
 * Each pixel's level L (0 = black .. 3 = white) is split across the two data planes, 1 bit per
 * pixel, most significant pixel first, neither inverted:
 *   DTM1 (0x10) <- high bit of L
 *   DTM2 (0x13) <- low bit of L
 */
class EPaperUC8179Gray4 : public EPaperGray4<EPaperUltraChip> {
 public:
  EPaperUC8179Gray4(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                    size_t init_sequence_length)
      : EPaperGray4(name, width, height, init_sequence, init_sequence_length, DISPLAY_TYPE_GRAYSCALE, 2) {}

 protected:
  bool initialise(bool partial) override;
  bool transfer_data() override;
  void refresh_screen(bool partial) override;
  void power_on() override;
  void power_off() override;
};

}  // namespace esphome::epaper_spi
