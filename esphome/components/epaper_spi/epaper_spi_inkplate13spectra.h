#pragma once

#include "epaper_spi_dualcs.h"

namespace esphome::epaper_spi {

/**
 * Soldered Inkplate 13 Spectra: 1200x1600 6-color e-paper on a 13.3" Spectra 6 panel with two
 * controller chips. BS0/BS1 select the controllers' interface mode.
 *
 * reset() powers the panel on step by step. It runs in the RESET_END state, which EPaperBase
 * does not gate on the busy pin: the busy pin of an unpowered panel reads as busy.
 */
class EPaperInkplate13Spectra : public EPaperDualCS {
 public:
  using EPaperDualCS::EPaperDualCS;

  void set_bs0_pin(GPIOPin *bs0) { this->bs0_pin_ = bs0; }
  void set_bs1_pin(GPIOPin *bs1) { this->bs1_pin_ = bs1; }

  void setup() override;
  void dump_config() override;

 protected:
  // The steps of reset()
  enum ResetSub {
    RST_PINS_LOW,
    RST_PINS_LOW_WAIT,  // 500 ms
    RST_IO_WAIT,        // 100 ms after PWR_EN goes high
    RST_LOW_WAIT,       // 100 ms RST low
    RST_HIGH_WAIT,      // 100 ms RST high
    RST_DONE,
  };

  bool reset() override;
  void refresh_screen(bool partial) override;
  bool after_refresh() override;
  void deep_sleep() override;

  /// Leaves the panel unpowered: unlike the base, nothing is driven high.
  void setup_pins() const override;
  void set_io_pins_();
  void set_all_pins_low_() const;

  /// A real full refresh keeps the panel busy for about 20 s. True when a full refresh command
  /// is pending and the busy pin cleared much sooner, meaning the panel ignored the command (for
  /// example because it never powered up), which would otherwise be reported as a normal update.
  bool refresh_was_ignored_() const;
  uint32_t refresh_elapsed_ms_() const;

  GPIOPin *bs0_pin_{nullptr};
  GPIOPin *bs1_pin_{nullptr};

  uint32_t refresh_started_ms_{0};
  ResetSub reset_sub_{RST_PINS_LOW};
  bool refresh_pending_{false};  // a full refresh command was sent and not yet checked
};

}  // namespace esphome::epaper_spi
