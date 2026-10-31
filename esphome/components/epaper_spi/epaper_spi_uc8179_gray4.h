#pragma once

#include "epaper_spi_uc8179.h"

namespace esphome::epaper_spi {

/**
 * Four-level grayscale for UC8179 panels such as the Waveshare 7.5" V2 (EPD_7in5_V2).
 *
 * The framebuffer packs 2 bits per pixel (4 per byte, most significant pixel first, 0 = black,
 * 3 = white). Each pixel's level is split across the controller's two 1-bit RAM planes, and the
 * panel's own OTP grayscale waveform is selected by forcing the temperature register, following
 * the Waveshare EPD_7IN5_V2_Init_4Gray example. Panels made before late 2023 may lack this waveform.
 *
 * The grayscale waveform has no partial form, so every update is a full refresh.
 */
class EPaperUC8179Gray4 : public EPaperUC8179 {
 public:
  EPaperUC8179Gray4(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                    size_t init_sequence_length)
      : EPaperUC8179(name, width, height, init_sequence, init_sequence_length, DISPLAY_TYPE_GRAYSCALE, 2) {}

  void fill(Color color) override;
  void draw_pixel_at(int x, int y, Color color) override;

 protected:
  static constexpr uint8_t CMD_BOOSTER_SOFT_START = 0x06;
  // Forced temperature that selects the OTP four-level waveform
  static constexpr uint8_t TSSET_GRAY4 = 0x5F;

  bool transfer_data() override;
};

}  // namespace esphome::epaper_spi
