#pragma once

#include "epaper_spi.h"

namespace esphome::epaper_spi {

/**
 * Waveshare 3-color e-paper displays (UC8179 and UC8151D controllers).
 * Supports: 7.5" V2 BWR (EPD_7in5b_V2), 800x480 pixels; 2.13" B V3 (EPD_2in13b_V3), 104x212 pixels.
 *
 * Color scheme: Black, White, Red (BWR)
 * Buffer layout: 1 bit per pixel, separate planes
 * - Buffer first half: Black/White plane (1=black, 0=white)
 * - Buffer second half: Red plane (1=red, 0=no red)
 * - Total buffer: width * height / 4 bytes (2 * width * height / 8)
 *
 * The init sequence (INITIALISE state) sends the power and panel configuration.
 * Power-on (0x04) is sent in the POWER_ON state after data transfer;
 * the state machine then busy-waits before triggering REFRESH_SCREEN (0x12).
 */
class EPaperWaveshareBWR : public EPaperBase {
 public:
  EPaperWaveshareBWR(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                     size_t init_sequence_length)
      : EPaperBase(name, width, height, init_sequence, init_sequence_length, DISPLAY_TYPE_BINARY, 1, 2) {}

 protected:
  void fill_buffer(Color color) override;
  void draw_pixel_at(int x, int y, Color color) override;
  bool transfer_data() override;
  void refresh_screen(bool partial) override;
  void power_on() override;
  void power_off() override;
  void deep_sleep() override;
};

}  // namespace esphome::epaper_spi
