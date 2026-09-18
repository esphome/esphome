#pragma once

#include "colorconv.h"
#include "epaper_spi_mono.h"

namespace esphome::epaper_spi {

/**
 * Four-level grayscale for SSD1677 panels. Full refresh only: the panel's OTP grayscale waveform
 * has no lighter/quicker mode, and master activation redraws the whole panel regardless of the RAM
 * window, so there is no way to do a partial update without discarding gray levels across the
 * whole screen, not just the changed area.
 *
 * The SSD1677 has two independent 1-bit RAM planes, normally used for a
 * black/white and a red plane. This class writes image data to both,
 * splitting each pixel's 2-bit gray level across them, and triggers the
 * panel's own OTP grayscale waveform instead of the normal monochrome update
 * sequence. No custom LUT upload needed for any currently supported panel,
 * the OTP waveform is used instead.
 *
 * The framebuffer therefore packs 2 bits per pixel (4 per byte, most
 * significant pixel first) instead of EPaperMono's 1 bit; everything else -
 * window setup, reset and sleep handling - is inherited unchanged.
 */
class EPaperSSD1677Gray4 : public EPaperMono {
 public:
  EPaperSSD1677Gray4(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                     size_t init_sequence_length)
      : EPaperMono(name, width, height, init_sequence, init_sequence_length, DISPLAY_TYPE_GRAYSCALE) {
    this->row_width_ = (width + 3) / 4;  // 4 pixels per byte
    this->buffer_length_ = (size_t) this->row_width_ * height;
  }

  void fill(Color color) override;
  void draw_pixel_at(int x, int y, Color color) override;

 protected:
  void refresh_screen(bool partial) override;
  bool transfer_data() override;
};

}  // namespace esphome::epaper_spi
