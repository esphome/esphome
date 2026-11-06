#pragma once

#include "epaper_weact_3c.h"

namespace esphome::epaper_spi {

/**
 * SSD1677 3-color (black/white/red) e-paper displays, e.g. the Waveshare 7.5" HD (B).
 *
 * Drawn and sent as EPaperWeAct3C, except that the SSD1677 addresses RAM X in pixels with two bytes, and the RAM is
 * written from the last gate down (data entry mode 0x01), as in Waveshare's reference code. The controller scans
 * offset_height + height gates and the panel is on the last height of them, so the RAM window covers only those.
 * The window is exactly the panel, so after the first plane the address counter wraps back to the start for the
 * second.
 */
class EPaperSSD1677BWR : public EPaperWeAct3C {
 public:
  EPaperSSD1677BWR(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                   size_t init_sequence_length, uint16_t offset_height)
      : EPaperWeAct3C(name, width, height, init_sequence, init_sequence_length), offset_height_(offset_height) {}

 protected:
  bool reset() override;
  void set_window() override;
  // The full refresh (0x22 0xF7) powers the panel up itself, as Waveshare's reference code relies on.
  void power_on() override {}
  void refresh_screen(bool partial) override;

  uint16_t offset_height_;
};

}  // namespace esphome::epaper_spi
