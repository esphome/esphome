#pragma once

#include "colorconv.h"
#include "epaper_spi_ssd1677.h"

namespace esphome::epaper_spi {

/**
 * Four-level grayscale for SSD1677 panels.
 *
 * The SSD1677 has two independent 1-bit RAM planes, normally used for a
 * black/white and a red plane. This class writes image data to both,
 * splitting each pixel's 2-bit gray level across them, and triggers the
 * panel's own OTP grayscale waveform instead of the normal monochrome update
 * sequence. No custom LUT upload needed for any currently supported panel,
 * the OTP waveform is used instead.
 *
 * The framebuffer therefore packs 2 bits per pixel (4 per byte, most
 * significant pixel first) instead of EPaperMono's 1 bit.
 *
 * The grayscale waveform has no partial form: master activation redraws the whole panel
 * regardless of the RAM window. So a full update is a four-level refresh, and when partial updates
 * are enabled (full_update_every > 1, which the model only allows on explicit request) a partial
 * update is EPaperSSD1677's black-and-white one, each pixel reduced to light or dark. The partial
 * waveform also drives unchanged pixels towards black or white, so from the first partial update
 * the whole panel loses its gray levels until the next full update.
 */
class EPaperSSD1677Gray4 : public EPaperSSD1677 {
 public:
  EPaperSSD1677Gray4(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                     size_t init_sequence_length)
      : EPaperSSD1677(name, width, height, init_sequence, init_sequence_length, DISPLAY_TYPE_GRAYSCALE) {
    this->row_width_ = (width + 3) / 4;  // 4 pixels per byte
    this->buffer_length_ = (size_t) this->row_width_ * height;
  }

  void fill(Color color) override;
  void draw_pixel_at(int x, int y, Color color) override;

 protected:
  // A partial update if partial updates are enabled and this is not a full one.
  bool is_partial_push_() const { return this->update_count_ != 0 && this->sent_.is_valid(); }
  void plane_row(size_t y, uint8_t *out) override;
  void refresh_screen(bool partial) override;
  bool transfer_data() override;
};

}  // namespace esphome::epaper_spi
