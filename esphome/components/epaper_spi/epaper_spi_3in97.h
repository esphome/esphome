#pragma once

#include "epaper_spi.h"

namespace esphome::epaper_spi {

// Waveshare 3.97" e-Paper display driver
// Resolution: 800x480

class EPaper3in97 : public EPaperBase {
 public:
  EPaper3in97(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
              size_t init_sequence_length)
      : EPaperBase(name, width, height, init_sequence, init_sequence_length, DISPLAY_TYPE_BINARY) {
    this->row_width_ = (width + 7) / 8;
    this->buffer_length_ = this->row_width_ * height;
  }

  void fill(Color color) override;

 protected:
  void draw_pixel_at(int x, int y, Color color) override;

  bool reset() override;
  bool initialise(bool partial) override;
  bool transfer_data() override;
  void refresh_screen(bool partial) override;
  void power_on() override {}
  void power_off() override {}
  void deep_sleep() override;

  void send_command_(uint8_t value);
  void send_data_(uint8_t value);
  bool wait_busy_demo_();
};

}  // namespace esphome::epaper_spi
