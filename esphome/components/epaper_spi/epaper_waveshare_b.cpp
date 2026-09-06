#include "epaper_waveshare_b.h"

namespace esphome::epaper_spi {

bool EpaperWaveshareB::reset() {
  if (EPaperBase::reset()) {
    this->command(0x12);
    return true;
  }
  return false;
}

}  // namespace esphome::epaper_spi
