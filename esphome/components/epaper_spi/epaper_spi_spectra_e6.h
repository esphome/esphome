#pragma once

#include "epaper_spi_4bpp.h"

namespace esphome::epaper_spi {

class EPaperSpectraE6 : public EPaper4bpp {
 public:
  using EPaper4bpp::EPaper4bpp;

  /**
   * Render the display lambda as usual, then transfer and refresh only the
   * given logical rectangle. Returns true when an async update was accepted.
   * Must be called from an automation lambda, not from inside the display
   * render lambda, and only when idle with a full baseline already shown.
   * Coordinates are clipped to the canvas and the physical X range is
   * widened outward to a multiple of 4 pixels (controller granularity).
   */
  bool partial_update(int x, int y, int w, int h);
  /// Normal updates always do a full refresh (drops a stale partial request
  /// when idle).
  void update() override;

 protected:
  uint8_t color_to_native(Color color) override;
  bool transfer_data() override;
  void refresh_screen(bool partial) override;
  void power_on() override;
  void power_off() override;
  void deep_sleep() override;

  void send_partial_window_();

  // Active partial rectangle in native (physical) pixels.
  uint16_t partial_x_{0};
  uint16_t partial_y_{0};
  uint16_t partial_w_{0};
  uint16_t partial_h_{0};
  bool partial_active_{false};
  bool full_baseline_done_{false};
};

}  // namespace esphome::epaper_spi
