#pragma once

#include "epaper_spi.h"

namespace esphome::epaper_spi {

/// Waveshare 7.3 inch e-Paper HAT (G): BWYR, vendor epd7in3g protocol; framebuffer 2 bpp, four pixels per byte.
class EPaperEpd7In3G final : public EPaperBase {
 public:
  EPaperEpd7In3G(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                 size_t init_sequence_length)
      : EPaperBase(name, width, height, init_sequence, init_sequence_length, DISPLAY_TYPE_COLOR) {
    this->buffer_length_ = static_cast<size_t>((width / 4) * height);
  }

  void fill(Color color) override;
  void clear() override;

 protected:
  void draw_pixel_at(int x, int y, Color color) override;
  bool transfer_data() override;
  void power_on() override;
  void refresh_screen(bool partial) override;
  void power_off() override;
  void deep_sleep() override;

 private:
  /** @name IC command / payload bytes (@c epd7in3g reference) @{ */
  static constexpr uint8_t CMD_BOOSTER_SOFTSTART = 0x04;
  static constexpr uint8_t CMD_POWEROFF = 0x02;
  static constexpr uint8_t CMD_DEEPSLEEP = 0x07;
  static constexpr uint8_t CMD_TRANSFER = 0x10;
  static constexpr uint8_t CMD_REFRESH = 0x12;
  static constexpr uint8_t DATA_POWEROFF = 0x00;
  static constexpr uint8_t DATA_DEEPSLEEP_KEY = 0xA5;
  static constexpr uint8_t DATA_REFRESH = 0x01;
  /** @} */

  uint8_t xfer_phase_{0};
};

}  // namespace esphome::epaper_spi
