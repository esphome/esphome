#pragma once

#include "epaper_spi.h"

namespace esphome::epaper_spi {

class EPaperSSD1677 : public EPaperBase {
 public:
  EPaperSSD1677(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                size_t init_sequence_length)
      : EPaperBase(name, width, height, init_sequence, init_sequence_length, DISPLAY_TYPE_BINARY) {
    this->buffer_length_ = width * height / 8;  // 8 pixels per byte
  }

 protected:
  void refresh_screen(bool partial) override;
  void power_on() override {}
  void power_off() override{};
  void deep_sleep() override;
  bool reset() override;
  bool transfer_data() override;
  bool send_red_{true};
};

}  // namespace esphome::epaper_spi
