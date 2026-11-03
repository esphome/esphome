#pragma once

#include "colorconv.h"
#include "epaper_spi_uc8179.h"

namespace esphome::epaper_spi {

/**
 * Four-level grayscale for UC8179 panels (Waveshare 7.5" V2), following Waveshare's EPD_7in5_V2
 * 4-gray sample.
 *
 * The framebuffer packs 2 bits per pixel (4 per byte, most significant pixel first). A full update
 * sends each level's high bit to DTM1 (0x10) and its low bit to DTM2 (0x13) and refreshes with the
 * panel's OTP four-level waveform.
 *
 * With partial updates enabled (full_update_every > 1), a partial update is a black-and-white
 * differential refresh with the register LUTs from the model's init sequence: DTM1 gets the frame
 * on the panel, DTM2 the new one, each pixel reduced to light or dark. The LUTs leave pixels that
 * stay light or stay dark undriven, so unchanged pixels keep their gray level. The frame on the panel
 * is kept in sent_ rather than in controller RAM, so the panel is deep slept after every update.
 */
class EPaperUC8179Gray4 : public EPaperUC8179 {
 public:
  EPaperUC8179Gray4(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                    size_t init_sequence_length)
      : EPaperUC8179(name, width, height, init_sequence, init_sequence_length, DISPLAY_TYPE_GRAYSCALE, 2) {}

  void setup() override;
  void fill(Color color) override;
  void draw_pixel_at(int x, int y, Color color) override;

 protected:
  static constexpr uint8_t CMD_PSR = 0x00;

  // A partial update if partial updates are enabled and this is not a full one.
  bool is_partial_push_() const { return this->update_count_ != 0 && this->sent_.is_valid(); }
  // Bytes in one 1 bit per pixel plane
  size_t plane_length_() const { return (size_t) (this->width_ / 8) * this->height_; }
  void set_gray4_mode_(bool partial);
  bool transfer_data() override;
  // Every update writes both planes, so nothing in controller RAM needs to survive
  void deep_sleep() override { this->send_deep_sleep_(); }
};

}  // namespace esphome::epaper_spi
