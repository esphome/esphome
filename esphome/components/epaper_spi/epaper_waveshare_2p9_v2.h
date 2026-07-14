#pragma once

#include "epaper_waveshare.h"

namespace esphome::epaper_spi {

/**
 * Waveshare 2.9" V2 Rev 2.1 (128x296, SSD1680-class) mono e-paper.
 *
 * Aligns with WaveshareEPaper2P9InV2R2 (waveshare_epaper model 2.90inv2-r2):
 * full refresh uses OTP waveform (0xF7), partial needs command 0x37 after the LUT,
 * and deep sleep between updates is skipped so controller RAM stays valid.
 */
class EpaperWaveshare2P9V2 final : public EpaperWaveshare {
 public:
  using EpaperWaveshare::EpaperWaveshare;

 protected:
  bool reset() override;
  bool initialise(bool partial) override;
  void refresh_screen(bool partial) override;
  void deep_sleep() override {}
  bool transfer_data() override;

  // Full/base update: after 0x24, also write the same buffer to 0x26.
  bool write_prev_plane_{false};
  // True while the second (0x26) transfer pass is in progress.
  bool transferring_prev_plane_{false};
};

}  // namespace esphome::epaper_spi
