#pragma once

#include "epaper_spi_ssd1683.h"

namespace esphome::epaper_spi {

// Partial refreshes compare RAM 0x24 (new image) with RAM 0x26 (image on the panel) and drive only changed pixels.
class EPaperSSD1681 : public EPaperSSD1683 {
 public:
  EPaperSSD1681(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                size_t init_sequence_length)
      : EPaperSSD1683(name, width, height, init_sequence, init_sequence_length) {
    // 0x12 puts the display option register back to its default, which turns off the RAM ping-pong the panel's
    // OTP enables; without it the controller no longer carries 0x24 over to 0x26 after a partial refresh
    this->software_reset_ = false;
  }

 protected:
  // Deep sleep mode 1 keeps the RAM and the hardware reset that leaves it keeps the RAM ping-pong, so the
  // image a partial refresh compares against survives the controller's deep sleep
  bool image_survives_sleep() const override { return true; }
  bool transfer_data() override;
  void refresh_screen(bool partial) override;
};

}  // namespace esphome::epaper_spi
