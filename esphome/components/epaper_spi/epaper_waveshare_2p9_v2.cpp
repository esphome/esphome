#include "epaper_waveshare_2p9_v2.h"

namespace esphome::epaper_spi {

bool EpaperWaveshare2P9V2::initialise(bool partial) {
  if (!partial) {
    return EpaperWaveshare::initialise(false);
  }
  this->send_init_sequence_(this->init_sequence_, this->init_sequence_length_);
  this->cmd_data(0x32, this->partial_lut_, this->partial_lut_length_);
  this->cmd_data(0x37, {0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00});
  this->cmd_data(0x3C, {0x80});
  this->cmd_data(0x22, {0xC0});
  this->command(0x20);
  this->next_delay_ = 100;
  this->send_red_ = true;
  return true;
}

}  // namespace esphome::epaper_spi
