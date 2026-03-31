#pragma once

#include "epaper_spi.h"

namespace esphome::epaper_spi {

/**
 * UC8179-based tri-color (Black/White/Red) e-paper displays.
 *
 * This driver handles larger BWR panels using the UC8179 controller,
 * such as the XSRUPB 2025 panel at 800x480.
 *
 * Color scheme: Black, White, Red (BWR)
 * Buffer layout: 1 bit per pixel, separate planes
 * - Buffer first half: Black/White plane (0=black, 1=white)
 * - Buffer second half: Red plane (polarity depends on invert_red setting)
 * - Total buffer: width * height / 4 bytes (2 * width * height / 8)
 *
 * Commands:
 * - 0x10: B/W data transmission
 * - 0x13: Red data transmission
 * - 0x04: Power on
 * - 0x12: Display refresh
 * - 0x02: Power off
 * - 0x07: Deep sleep (with 0xA5 parameter)
 */
class EPaperUC8179BWR : public EPaperBase {
 public:
  EPaperUC8179BWR(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                  size_t init_sequence_length, bool invert_red)
      : EPaperBase(name, width, height, init_sequence, init_sequence_length, DISPLAY_TYPE_BINARY),
        invert_red_(invert_red) {
    this->buffer_length_ = this->row_width_ * height * 2;
  }

  void fill(Color color) override;
  void clear() override;
  void loop() override;

 protected:
  bool reset() override;
  void refresh_screen(bool partial) override;
  void power_on() override;
  void power_off() override;
  void deep_sleep() override;
  void draw_pixel_at(int x, int y, Color color) override;
  bool transfer_data() override;

  bool invert_red_{false};
};

}  // namespace esphome::epaper_spi
