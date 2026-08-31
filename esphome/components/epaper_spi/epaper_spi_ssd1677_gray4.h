#pragma once

#include "epaper_spi_mono.h"

namespace esphome::epaper_spi {

/**
 * SSD1677 four-level grayscale using the panel's factory OTP waveform instead of a
 * microcontroller-uploaded lookup table. Each pixel's two bits are split across the controller's two
 * RAM planes (command 0x24 and command 0x26); the combination selects one of the four waveform slots
 * already burned into the panel at the factory (SSD1677 Rev 1.0 Table 6-4). No `Write LUT register`
 * (0x32) command is ever sent.
 *
 * Partial refresh is not supported: grayscale plus partial refresh is an open problem across this
 * controller's ecosystem (ghosting/incomplete anti-aliasing), so every update redraws the full frame.
 */
class EPaperSSD1677Gray4 : public EPaperMono {
 public:
  EPaperSSD1677Gray4(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                      size_t init_sequence_length)
      : EPaperMono(name, width, height, init_sequence, init_sequence_length) {
    // Two 1-bit planes, back to back: [0, plane_bytes) goes to command 0x24, [plane_bytes, 2*plane_bytes)
    // goes to command 0x26.
    this->buffer_length_ = this->row_width_ * height * 2;
  }

  void fill(Color color) override;

 protected:
  void draw_pixel_at(int x, int y, Color color) override;
  void refresh_screen(bool partial) override;
  void set_window() override;
  bool transfer_data() override;
  void deep_sleep() override;

  /// Quantizes an RGB color to one of four gray levels: 0 = black .. 3 = white.
  static uint8_t color_to_gray_(Color color);
};

}  // namespace esphome::epaper_spi
