#pragma once

#include <cstddef>
#include <cstdint>

#include "epaper_spi_mono.h"

namespace esphome::epaper_spi {

/**
 * Waveshare 2.9" V2 Rev 2.1 (128x296, SSD1680-class) mono e-paper.
 *
 * A full refresh uses the controller's built-in OTP waveform, so no full LUT is sent. A partial
 * refresh loads a LUT and then sends command 0x37, as in the Waveshare SDK. The controller is
 * never put into deep sleep between updates, so its RAM keeps the previous frame for partial
 * refresh.
 */
class EpaperWaveshare2P9V2 final : public EPaperMono {
 public:
  EpaperWaveshare2P9V2(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                       size_t init_sequence_length, const uint8_t *partial_lut, size_t partial_lut_length)
      : EPaperMono(name, width, height, init_sequence, init_sequence_length),
        partial_lut_(partial_lut),
        partial_lut_length_(partial_lut_length) {}

 protected:
  bool reset() override;
  bool initialise(bool partial) override;
  void set_window() override;
  void refresh_screen(bool partial) override;
  void deep_sleep() override {}
  bool transfer_data() override;

  const uint8_t *partial_lut_;
  size_t partial_lut_length_;

  // Full/base update: after 0x24, also write the same buffer to 0x26.
  bool write_prev_plane_{false};
  // True while the second (0x26) transfer pass is in progress.
  bool transferring_prev_plane_{false};
};

}  // namespace esphome::epaper_spi
