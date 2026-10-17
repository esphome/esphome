#pragma once

#include "epaper_spi_mono.h"

namespace esphome::epaper_spi {

/**
 * Monochrome SSD1680 display with differential partial refresh support.
 *
 * The WeAct SSD1680 variant uses the monochrome RAM layout and a dedicated
 * partial waveform LUT.
 */
class EPaperSSD1680 : public EPaperMono {
 public:
  EPaperSSD1680(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                size_t init_sequence_length)
      : EPaperMono(name, width, height, init_sequence, init_sequence_length) {}

  void setup() override;

 protected:
  bool initialise(bool partial) override;
  void power_on() override;
  void power_off() override;
  void refresh_screen(bool partial) override;
  void set_window() override;
  void deep_sleep() override;
  bool transfer_data() override;

  split_buffer::SplitBuffer sent_{};
  uint8_t plane_{0};
};

}  // namespace esphome::epaper_spi
