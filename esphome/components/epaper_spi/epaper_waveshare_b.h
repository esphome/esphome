#pragma once
#include "epaper_weact_3c.h"

namespace esphome::epaper_spi {

/**
 * Waveshare (B) series BWR e-paper displays using SSD1680-compatible controllers.
 * Waveshare uses 0=red, 1=no-red, the inverse of EPaperWeAct3C
 */
class EpaperWaveshareB : public EPaperWeAct3C {
 public:
  EpaperWaveshareB(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                   size_t init_sequence_length)
      : EPaperWeAct3C(name, width, height, init_sequence, init_sequence_length, /*invert_red=*/true) {}

 protected:
  bool reset() override;
};

}  // namespace esphome::epaper_spi
