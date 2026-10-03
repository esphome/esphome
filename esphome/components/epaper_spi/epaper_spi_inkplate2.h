#pragma once

#include "epaper_spi_bwr.h"

namespace esphome::epaper_spi {

// Soldered Inkplate 2: 104x212 black/white/red (BWR) e-paper, UC8xxx-family controller.
// The init sequence sets DDX=11, so the red plane is sent inverted.
class EPaperInkplate2 final : public EPaperBWR {
 public:
  EPaperInkplate2(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                  size_t init_sequence_length)
      : EPaperBWR(name, width, height, init_sequence, init_sequence_length, /*invert_red=*/true) {}

 protected:
  void refresh_screen(bool partial) override;
  void power_on() override;
  void power_off() override;
  void deep_sleep() override;
};

}  // namespace esphome::epaper_spi
