#pragma once

#include "epaper_spi_ssd1683.h"

namespace esphome::epaper_spi {

// Partial refreshes compare RAM 0x24 (new image) with RAM 0x26 (image on the panel) and drive only changed pixels.
class EPaperSSD1681 : public EPaperSSD1683 {
 public:
  EPaperSSD1681(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                size_t init_sequence_length)
      : EPaperSSD1683(name, width, height, init_sequence, init_sequence_length) {
    // 0x12 puts the display option register back to its default, dropping the settings the panel's OTP loads into it
    this->software_reset_ = false;
  }

 protected:
  // Writes the previous frame to 0x26 before a partial refresh
  bool needs_previous_frame() const override { return true; }
  // Sends the frame last sent to 0x26, in chunks; false until it is all sent
  bool transfer_previous_frame_();
  bool transfer_data() override;
  void refresh_screen(bool partial) override;
};

}  // namespace esphome::epaper_spi
