#pragma once

#include "epaper_spi_dualcs.h"

namespace esphome::epaper_spi {

/**
 * T133A01-based 6-color e-paper display driver. Everything but reset() comes from EPaperDualCS.
 */
class EPaperT133A01 : public EPaperDualCS {
 public:
  using EPaperDualCS::EPaperDualCS;

 protected:
  bool reset() override;
};

}  // namespace esphome::epaper_spi
