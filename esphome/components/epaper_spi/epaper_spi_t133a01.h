#pragma once

#include "epaper_spi_dualcs.h"

namespace esphome::epaper_spi {

/**
 * T133A01-based 6-color e-paper display driver.
 *
 * Chip-select routing, pixel packing/color, and the full-refresh/partial-update
 * transfer_data() all come from EPaperDualCS/EPaper4bpp -- transfer_data() is EPaperDualCS's
 * shared implementation of this class's original algorithm (CS-held-low-across-yields, with
 * an off-by-one deadlock fix for issue #17668, see
 * tests/components/epaper_spi/display/test_t133a01_transfer.cpp), now also confirmed
 * working on Inkplate 13 Spectra hardware. This class supplies only the GPIO power-on
 * bring-up, the panel's register init table, and the small power/refresh/sleep commands.
 */
class EPaperT133A01 : public EPaperDualCS {
 public:
  using EPaperDualCS::EPaperDualCS;

 protected:
  bool reset() override;
  bool initialise(bool partial) override;
  void refresh_screen(bool partial) override;
  void power_on() override;
  void power_off() override;
  void deep_sleep() override;

  uint8_t color_to_native(Color color) override;
};

}  // namespace esphome::epaper_spi
