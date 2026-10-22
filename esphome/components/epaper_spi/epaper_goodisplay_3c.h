#pragma once

#include "epaper_weact_3c.h"

namespace esphome::epaper_spi {

/**
 * Good Display 3-color e-paper displays (SSD1680 controller), e.g. GDEY029Z95.
 *
 * Driven as EPaperWeAct3C, except that there is no separate power-on: the full refresh (0xF7) powers the panel
 * up itself. Busy then only goes high a little after the refresh starts, so the next state waits before
 * checking it.
 */
class EPaperGoodDisplay3C : public EPaperWeAct3C {
 public:
  using EPaperWeAct3C::EPaperWeAct3C;

 protected:
  // Time from master activation (0x20) until busy can be relied on
  static constexpr uint16_t REFRESH_START_MS = 50;

  // With EPaperWeAct3C's power-on (0x22 0xF8) first, this panel's refresh ends almost at once and the image
  // does not change.
  void power_on() override;
  void refresh_screen(bool partial) override;
};

}  // namespace esphome::epaper_spi
