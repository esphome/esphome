#pragma once
#include "epaper_spi.h"
#include "epaper_spi_mono.h"

namespace esphome::epaper_spi {
/**
 * An epaper display that needs LUTs to be sent to it.
 */
class EpaperWaveshare : public EPaperMono {
 public:
  EpaperWaveshare(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                  size_t init_sequence_length, const uint8_t *lut, size_t lut_length, const uint8_t *partial_lut,
                  uint16_t partial_lut_length)
      : EPaperMono(name, width, height, init_sequence, init_sequence_length),
        lut_(lut),
        lut_length_(lut_length),
        partial_lut_(partial_lut),
        partial_lut_length_(partial_lut_length) {}

 protected:
  void initialise(bool partial) override;
  void set_window() override;
  void refresh_screen(bool partial) override;
  void deep_sleep() override;
  const uint8_t *lut_;
  size_t lut_length_;
  const uint8_t *partial_lut_;
  uint16_t partial_lut_length_;
};
}  // namespace esphome::epaper_spi
