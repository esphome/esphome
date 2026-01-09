#include "epaper_waveshare.h"

namespace esphome::epaper_spi {
bool EpaperWaveshare::reset() {
  if (EPaperBase::reset()) {
    this->command(0x12);
    return true;
  }
  return false;
}

void EpaperWaveshare::refresh_screen(bool partial) {
  if (partial) {
    this->cmd_data(0x32, this->partial_lut_, this->partial_lut_length_);
    this->cmd_data(0x3C, {0x80});
    this->cmd_data(0x22, {0x0F});
  } else {
    this->cmd_data(0x32, this->lut_, this->lut_length_);
    this->cmd_data(0x3C, {0x05});
    this->cmd_data(0x22, {0xC7});
  }
  this->command(0x20);
}

void EpaperWaveshare::deep_sleep() { this->cmd_data(0x10, {0x01}); }
}  // namespace esphome::epaper_spi
