#include "epaper_waveshare.h"

namespace esphome::epaper_spi {
bool EpaperWaveshare::reset() {
  if (EPaperBase::reset()) {
    this->command(0x12);
    return true;
  }
  return false;
}

void EpaperWaveshare::initialise(bool partial) {
  EPaperBase::initialise(partial);
  if (partial) {
    this->cmd_data(0x32, this->partial_lut_, this->partial_lut_length_);
    this->cmd_data(0x3C, {0x80});
    this->cmd_data(0x22, {0xC0});
    this->command(0x20);
    this->delay_until_ = 100;
  } else {
    this->cmd_data(0x32, this->lut_, this->lut_length_);
    this->cmd_data(0x3C, {0x05});
  }
  this->send_red_ = true;
}

void EpaperWaveshare::set_window_() {
  this->x_low_ &= ~7;
  this->x_high_ += 7;
  this->x_high_ &= ~7;
  uint16_t x_start = this->x_low_ / 8;
  uint16_t x_end = (this->x_high_ - 1) / 8;
  cmd_data(0x44, {(uint8_t) x_start, (uint8_t) (x_end)});
  cmd_data(0x4E, {(uint8_t) x_start});
  cmd_data(0x45, {(uint8_t) this->y_low_, (uint8_t) (this->y_low_ / 256), (uint8_t) (this->y_high_ - 1),
                  (uint8_t) ((this->y_high_ - 1) / 256)});
  cmd_data(0x4F, {(uint8_t) this->y_low_, (uint8_t) (this->y_low_ / 256)});
}

void EpaperWaveshare::refresh_screen(bool partial) {
  if (partial) {
    this->cmd_data(0x22, {0x0F});
  } else {
    this->cmd_data(0x22, {0xC7});
  }
  this->command(0x20);
  this->delay_until_ = partial ? 100 : 3000;
}

void EpaperWaveshare::deep_sleep() { this->cmd_data(0x10, {0x01}); }
}  // namespace esphome::epaper_spi
