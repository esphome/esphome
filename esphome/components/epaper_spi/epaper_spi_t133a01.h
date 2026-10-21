#pragma once

#include "epaper_spi_el133uf1.h"

namespace esphome::epaper_spi {

/**
 * T133A01-based 6-color e-paper display driver. Everything but reset() comes from EPaperEL133UF1.
 */
class EPaperT133A01 : public EPaperEL133UF1 {
 public:
  using EPaperEL133UF1::EPaperEL133UF1;

 protected:
  bool reset() override;
};

}  // namespace esphome::epaper_spi
