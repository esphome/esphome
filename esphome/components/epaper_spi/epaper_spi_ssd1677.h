#pragma once

#include "epaper_spi_mono.h"

namespace esphome::epaper_spi {

/**
 * SSD1677 driver with an optional four-level grayscale mode, selected at runtime via
 * set_grayscale(). Black/white mode delegates to EPaperMono/EPaperBase unchanged.
 *
 * Grayscale uses the panel's factory OTP waveform instead of a microcontroller-uploaded lookup
 * table. Each pixel's two bits are split across the controller's two RAM planes (command 0x24 and
 * command 0x26); the combination selects one of the four waveform slots already burned into the
 * panel at the factory (SSD1677 Rev 1.0 Table 6-4). No `Write LUT register` (0x32) command is ever
 * sent. The exact register values (temperature trick, update sequence, plane mapping) are
 * device-specific and only confirmed for the Seeed reTerminal Sticky; do not enable this mode for
 * other SSD1677 boards without separately verifying it against their own factory firmware.
 *
 * Partial refresh is not supported in grayscale mode: grayscale plus partial refresh is an open
 * problem across this controller's ecosystem (ghosting/incomplete anti-aliasing), so every
 * grayscale update redraws the full frame.
 */
class EPaperSSD1677 : public EPaperMono {
 public:
  EPaperSSD1677(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                size_t init_sequence_length)
      : EPaperMono(name, width, height, init_sequence, init_sequence_length) {}

  void set_grayscale(bool grayscale) { this->grayscale_ = grayscale; }

  void setup() override {
    // set_grayscale() (from code generation) always runs before setup(), so grayscale_ already
    // holds its final configured value here. Only grow the buffer to two planes ([0, plane_bytes)
    // -> command 0x24, [plane_bytes, 2*plane_bytes) -> command 0x26) when actually needed, so
    // existing 1bpp configs don't pay for RAM they don't use.
    if (this->grayscale_) {
      this->buffer_length_ = this->row_width_ * this->height_ * 2;
    }
    EPaperBase::setup();
  }

  void fill(Color color) override;

  DisplayType get_display_type() override { return this->grayscale_ ? DISPLAY_TYPE_GRAYSCALE : DISPLAY_TYPE_BINARY; }

 protected:
  void draw_pixel_at(int x, int y, Color color) override;
  void refresh_screen(bool partial) override;
  void set_window() override;
  bool transfer_data() override;
  void deep_sleep() override;

  /// Quantizes an RGB color to one of four gray levels: 0 = black .. 3 = white.
  static uint8_t color_to_gray_(Color color);

  bool grayscale_{false};
};

}  // namespace esphome::epaper_spi
