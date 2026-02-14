#pragma once

#include "epaper_spi.h"

namespace esphome::epaper_spi {

/**
 * WeAct 3-color e-paper displays (SSD1683 controller).
 * Supports multiple sizes: 2.9" (128x296), 4.2" (400x300), etc.
 *
 * Color scheme: Black, White, Red (BWR)
 * Buffer layout: 1 bit per pixel, separate planes
 * - Buffer first half: Black/White plane (1=black, 0=white)
 * - Buffer second half: Red plane (1=red, 0=no red)
 * - Total buffer: width * height / 4 bytes (2 * width * height / 8)
 */
class EPaperWeAct3C : public EPaperBase {
 public:
  EPaperWeAct3C(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                size_t init_sequence_length)
      : EPaperBase(name, width, height, init_sequence, init_sequence_length, DISPLAY_TYPE_BINARY) {
    this->buffer_length_ = this->row_width_ * height * 2;
  }

  void fill(Color color) override;
  void clear() override;

 protected:
  void set_window_();
  void refresh_screen(bool partial) override;
  void power_on() override;
  void power_off() override;
  void deep_sleep() override;
  void draw_pixel_at(int x, int y, Color color) override;

  bool transfer_data() override;
};

}  // namespace esphome::epaper_spi
