#pragma once
#include "epaper_weact_3c.h"

namespace esphome::epaper_spi {

/**
 * Waveshare (B) series BWR e-paper displays using SSD1680-compatible controllers.
 * Waveshare uses 0=red, 1=no-red, the inverse of EPaperWeAct3C
 */
class EpaperWaveshareB : public EPaperWeAct3C {
 public:
  using EPaperWeAct3C::EPaperWeAct3C;

 protected:
  bool reset() override;
  uint8_t transform_red_byte(uint8_t byte) const override { return static_cast<uint8_t>(~byte); }
};

}  // namespace esphome::epaper_spi
