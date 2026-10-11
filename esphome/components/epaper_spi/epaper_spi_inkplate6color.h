#pragma once

#include "epaper_spi_4bpp.h"

namespace esphome::epaper_spi {

// Soldered Inkplate 6COLOR: 600x448 7-color (black/white/green/blue/red/yellow/orange) e-paper,
// UC8159-family controller.
class EPaperInkplate6Color final : public EPaper4bpp {
 public:
  using EPaper4bpp::EPaper4bpp;

 protected:
  uint8_t color_to_native(Color color) override;

  void refresh_screen(bool partial) override;
  void power_on() override;
  void power_off() override;
  void deep_sleep() override;
};

}  // namespace esphome::epaper_spi
