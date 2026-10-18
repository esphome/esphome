#pragma once

#include "epaper_gray4.h"
#include "epaper_spi_uc8179.h"

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
class EPaperUC8179Gray4 : public EPaperGray4<EPaperUC8179> {
 public:
  using EPaperGray4::EPaperGray4;

 protected:
  bool initialise(bool partial) override;
  bool transfer_data() override;
  bool init_powered_{false};  // the first step of initialise() has run for this update
};

}  // namespace esphome::epaper_spi
