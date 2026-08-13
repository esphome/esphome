#pragma once

#include "epaper_spi_dualcs.h"

namespace esphome::epaper_spi {

/**
 * Soldered Inkplate 13 Spectra: 1200x1600 6-color (black/white/yellow/red/blue/green)
 * e-paper, dual-chip controller (13.3" E Ink Spectra 6 panel). Dual-CS data transfer, chip-
 * select routing and PTLW-based partial update come from EPaperDualCS; the panel is split
 * into two halves, each with its own chip-select (CS: left half of every row, CS1: right
 * half). BS0/BS1 select the controllers' interface mode.
 *
 * GPIO power-on bring-up (all-pins-low, then IO setup + PWR_EN, then an RST pulse) runs
 * inside reset(), not initialise(). reset()/RESET_END is the one state EPaperBase never
 * busy-gates (see epaper_spi.h), so this sequence completes before the panel is powered
 * -- and before power-on, BUSY reads the same level as "genuinely busy", so gating on it
 * there would hang forever. initialise() only starts once the panel is actually powered,
 * so its busy-pin checks reflect a real signal.
 *
 * display_partial() is the public entry point for EPaperDualCS's partial-update machinery
 * -- verified working on this board (see EPaperDualCS for why it's not exposed by every
 * subclass).
 */
class EPaperInkplate13Spectra final : public EPaperDualCS {
 public:
  using EPaperDualCS::EPaperDualCS;

  void set_rst_pin(GPIOPin *p) { this->rst_pin_ = p; }
  void set_pwr_en_pin(GPIOPin *p) { this->pwr_en_pin_ = p; }
  void set_bs_pins(GPIOPin *bs0, GPIOPin *bs1) {
    this->bs0_pin_ = bs0;
    this->bs1_pin_ = bs1;
  }

  void setup() override;
  void dump_config() override;

  // Trigger a partial (subregion) update. x/y/w/h are in logical (rotated) coords.
  // The buffer must already contain the updated pixels before calling this.
  void display_partial(int x, int y, int w, int h) { this->start_partial_update_(x, y, w, h); }

 protected:
  // GPIO power-on bring-up, run inside reset()/RESET_END (see class-level comment).
  enum ResetSub {
    RST_PINS_LOW,
    RST_PINS_LOW_WAIT,  // 500 ms
    RST_IO_WAIT,        // 100 ms after PWR_EN goes high
    RST_LOW_WAIT,       // 100 ms RST low
    RST_HIGH_WAIT,      // 100 ms RST high
    RST_DONE,
  };

  bool reset() override;
  bool initialise(bool partial) override;
  void power_on() override;
  void refresh_screen(bool partial) override;
  void power_off() override;
  void deep_sleep() override;

  uint8_t color_to_native(Color color) override;

  void set_io_pins_();
  void set_all_pins_low_();

  /// Replays the panel register init table.
  void send_init_sequence_();

  GPIOPin *rst_pin_{nullptr};
  GPIOPin *pwr_en_pin_{nullptr};
  GPIOPin *bs0_pin_{nullptr};
  GPIOPin *bs1_pin_{nullptr};

  ResetSub reset_sub_{RST_PINS_LOW};
};

}  // namespace esphome::epaper_spi
