#pragma once

#include "epaper_spi_bwr.h"

namespace esphome::epaper_spi {

/**
 * UC8179-based tri-color (Black/White/Red) e-paper displays.
 *
 * This driver handles larger BWR panels using the UC8179 controller,
 * such as the XSRUPB 2025 panel at 800x480.
 *
 * Color scheme: Black, White, Red (BWR)
 * Buffer layout: 1 bit per pixel, separate planes
 * - Buffer first half: Black/White plane (1=black, 0=white or red)
 * - Buffer second half: Red plane (1=red, 0=no red)
 * - Total buffer: width * height / 4 bytes (2 * width * height / 8)
 * Panels with DDX=11 (as set by the model init sequence) need invert_red, which sends the red plane inverted.
 *
 * Commands:
 * - 0x04: Power on
 * - 0x10: B/W data transmission
 * - 0x13: Red data transmission
 * - 0x12: Display refresh
 * - 0x02: Power off
 * - 0x07: Deep sleep (with 0xA5 parameter)
 */
class EPaperUC8179BWR : public EPaperBWR {
 public:
  EPaperUC8179BWR(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                  size_t init_sequence_length, bool invert_red)
      : EPaperBWR(name, width, height, init_sequence, init_sequence_length, invert_red) {}

 protected:
  bool initialise(bool partial) override;
  void refresh_screen(bool partial) override;
  void power_on() override;
  void power_off() override;
  void deep_sleep() override;
};

}  // namespace esphome::epaper_spi
