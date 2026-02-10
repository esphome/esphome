#pragma once

#include "epaper_spi.h"

namespace esphome::epaper_spi {
/**
 * A class for Solomon SSD1683 epaper displays.
 */
class EPaperSSD1683 : public EPaperBase {
 public:
  EPaperSSD1683(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                size_t init_sequence_length)
      : EPaperBase(name, width, height, init_sequence, init_sequence_length, DISPLAY_TYPE_BINARY) {
    this->buffer_length_ = (width + 7) / 8 * height;  // 8 pixels per byte, rounded up
  }

 protected:
  void refresh_screen(bool partial) override;
  void power_on() override {}
  void power_off() override{};
  void deep_sleep() override;
  bool reset() override;
  virtual void set_window();
  bool transfer_data() override;
  bool send_red_{true};
};

}  // namespace esphome::epaper_spi
