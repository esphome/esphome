#include "epaper_spi_ssd1677_bwr.h"

namespace esphome::epaper_spi {

bool EPaperSSD1677BWR::reset() {
  if (!EPaperBase::reset())
    return false;
  this->command(0x12);  // SWRESET; the INITIALISE state waits for busy to clear
  return true;
}

void EPaperSSD1677BWR::set_window() {
  const uint16_t x_end = this->width_ - 1;
  const uint16_t y_first = this->offset_height_ + this->height_ - 1;
  const uint16_t y_last = this->offset_height_;
  this->cmd_data(0x44, {0x00, 0x00, (uint8_t) x_end, (uint8_t) (x_end >> 8)});
  // Y runs down from the last gate, to match data entry mode 0x01
  this->cmd_data(0x45, {(uint8_t) y_first, (uint8_t) (y_first >> 8), (uint8_t) y_last, (uint8_t) (y_last >> 8)});
  this->cmd_data(0x4E, {0x00, 0x00});
  this->cmd_data(0x4F, {(uint8_t) y_first, (uint8_t) (y_first >> 8)});
}

void EPaperSSD1677BWR::refresh_screen(bool partial) {
  this->cmd_data(0x22, {0xF7});
  this->command(0x20);
}

}  // namespace esphome::epaper_spi
