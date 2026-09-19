#pragma once

#include "epaper_spi.h"

namespace esphome::epaper_spi {

/**
 * Monochrome e-paper displays using the UC8179 controller.
 * Supports: 7.5" V2 (EPD_7in5_V2), 800x480 pixels, as used by the
 * Waveshare 7.5" V2 HAT and the Seeed reTerminal E1001.
 *
 * Buffer layout: 1 bit per pixel, 1=white, 0=black (the base class default).
 *
 * The INITIALISE state sends the panel configuration followed by power-on
 * (0x04); the state machine busy-waits for power-on to complete before
 * TRANSFER_DATA, which first writes the waveform/mode registers (these are
 * only accepted while powered) and then the image data. The state machine
 * busy-waits again before triggering REFRESH_SCREEN (0x12).
 *
 * Three refresh modes are used, following the Waveshare EPD_7in5_V2 examples:
 * - full_update_every == 1: plain full refresh. The new image is sent
 *   inverted to DTM2 (0x13) and the controller uses its normal waveform.
 * - full_update_every > 1, full update: fast full refresh. The data polarity
 *   is flipped via the VCOM/data-interval register, a fast waveform is forced
 *   via the temperature registers, and the image is sent to both DTM1 (0x10,
 *   inverted) and DTM2 (0x13) so that every pixel transitions.
 * - full_update_every > 1, partial update: partial refresh. A partial-update
 *   waveform is forced, partial mode is entered with a full-screen window and
 *   only DTM2 is sent; the controller compares against its previous-image RAM.
 */
class EPaperUC8179 final : public EPaperBase {
 public:
  EPaperUC8179(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
               size_t init_sequence_length)
      : EPaperBase(name, width, height, init_sequence, init_sequence_length, DISPLAY_TYPE_BINARY) {
    this->buffer_length_ = this->row_width_ * height;
  }

 protected:
  bool initialise(bool partial) override;
  bool transfer_data() override;
  void refresh_screen(bool partial) override;
  void power_on() override;
  void power_off() override;
  void deep_sleep() override;
  void set_refresh_mode_();

  // Set by initialise() so transfer_data() knows which planes to send
  bool partial_{};
};

}  // namespace esphome::epaper_spi
