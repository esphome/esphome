#pragma once

#include "epaper_spi.h"

namespace esphome::epaper_spi {

class EPaperSSD1677 final : public EPaperBase {
 public:
  EPaperSSD1677(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                size_t init_sequence_length)
      : EPaperBase(name, width, height, init_sequence, init_sequence_length, DISPLAY_TYPE_BINARY) {
    this->buffer_length_ = this->row_width_ * height;
  }

 protected:
  bool reset() override;
  bool initialise(bool partial) override;
  bool transfer_data() override;
  void refresh_screen(bool partial) override;
  void power_on() override {}
  void power_off() override {}
  void deep_sleep() override;

  void set_window_();
};

}  // namespace esphome::epaper_spi
