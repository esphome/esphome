#pragma once

#include "epaper_spi_jd79660.h"

namespace esphome::epaper_spi {

class EPaperWaveshare3P97InG final : public EPaperJD79660 {
 public:
  EPaperWaveshare3P97InG(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                         size_t init_sequence_length)
      : EPaperJD79660(name, width, height, init_sequence, init_sequence_length, nullptr, 0) {}

  void fill(Color color) override;

 protected:
  bool reset() override;
  bool initialise(bool partial) override;
  void refresh_screen(bool partial) override;
  void power_off() override;
};

}  // namespace esphome::epaper_spi
