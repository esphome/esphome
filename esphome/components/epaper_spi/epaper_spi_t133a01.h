#pragma once

#include "epaper_spi_dualcs.h"

namespace esphome::epaper_spi {

/**
 * T133A01-based 6-color e-paper display driver.
 *
 * Chip-select routing, pixel packing/color, the full-refresh/partial-update transfer_data()
 * and the power/refresh/sleep commands all come from EPaperDualCS/EPaper4bpp. This class
 * supplies only the GPIO power-on bring-up and the panel's register init table.
 */
class EPaperT133A01 : public EPaperDualCS {
 public:
  using EPaperDualCS::EPaperDualCS;

 protected:
  bool reset() override;
  bool initialise(bool partial) override;
};

}  // namespace esphome::epaper_spi
