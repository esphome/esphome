#pragma once
#include "epaper_weact_3c.h"

namespace esphome::epaper_spi {

/**
 * Waveshare (B) series BWR e-paper displays using SSD1680-compatible controllers.
 * Adds the soft reset the controller expects after a hardware reset. Red plane polarity
 * varies between panels in this series and is set per model via set_invert_red().
 */
class EpaperWaveshareB : public EPaperWeAct3C {
 public:
  using EPaperWeAct3C::EPaperWeAct3C;

 protected:
  bool reset() override;
};

}  // namespace esphome::epaper_spi
