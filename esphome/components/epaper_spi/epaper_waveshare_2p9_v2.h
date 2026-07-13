#pragma once

#include "epaper_waveshare.h"

namespace esphome::epaper_spi {

/**
 * Waveshare 2.9" V2 Rev 2.1 (128x296, SSD1680-class) mono e-paper.
 * Partial refresh requires command 0x37 after loading the partial LUT (Waveshare SDK).
 */
class EpaperWaveshare2P9V2 final : public EpaperWaveshare {
 public:
  using EpaperWaveshare::EpaperWaveshare;

 protected:
  bool initialise(bool partial) override;
};

}  // namespace esphome::epaper_spi
