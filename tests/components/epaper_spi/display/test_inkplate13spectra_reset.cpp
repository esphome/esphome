#include <gtest/gtest.h>

#include "../common.h"
#include "esphome/components/epaper_spi/epaper_spi_inkplate13spectra.h"

namespace esphome::epaper_spi::testing {

/// Exposes the power-up state machine in reset() so it can be driven directly.
class TestableInkplate13Spectra : public EPaperInkplate13Spectra {
 public:
  TestableInkplate13Spectra() : EPaperInkplate13Spectra("test", 8, 2, nullptr, 0, false) {}

  void install() {
    this->set_dc_pin(&this->dc);
    this->set_cs_pins(&this->cs, &this->cs1);
    this->set_busy_pin(&this->busy);
    this->set_rst_pin(&this->rst);
    this->set_pwr_en_pin(&this->pwr_en);
    this->set_bs_pins(&this->bs0, &this->bs1);
  }

  using EPaperInkplate13Spectra::reset;
  using EPaperInkplate13Spectra::reset_sub_;
  using EPaperInkplate13Spectra::state_;
  using EPaperInkplate13Spectra::RST_DONE;
  using EPaperInkplate13Spectra::RST_IO_WAIT;
  using EPaperInkplate13Spectra::RST_PINS_LOW_WAIT;

  RecordingPin dc, cs, cs1, busy, rst, pwr_en, bs0, bs1;
};

/// A cycle that ends without reaching deep_sleep() (for example one abandoned after a busy-pin
/// timeout) leaves the power-up state machine at RST_DONE. The next cycle must still run the
/// whole bring-up: drive every pin low to drain the panel, then switch the panel power on.
/// Before the fix, reset() saw RST_DONE and returned at once, so the next update sent its init
/// sequence to an unpowered panel.
TEST(EPaperInkplate13Spectra, NewCycleRunsFullPowerUpAfterAnUnfinishedOne) {
  TestableInkplate13Spectra display;
  display.install();

  // Left over from a cycle that never reached deep_sleep().
  display.reset_sub_ = TestableInkplate13Spectra::RST_DONE;

  // Start of a new cycle: EPaperBase calls reset() once in RESET.
  display.state_ = EPaperState::RESET;
  EXPECT_FALSE(display.reset()) << "reset() skipped the power-up sequence";
  EXPECT_EQ(display.reset_sub_, TestableInkplate13Spectra::RST_PINS_LOW_WAIT);
  EXPECT_FALSE(display.pwr_en.level) << "panel power should be off while the pins are held low";
  EXPECT_FALSE(display.cs.level) << "pins were not driven low";
  EXPECT_FALSE(display.busy.level) << "pins were not driven low";

  // The rest of the bring-up runs in RESET_END and must continue, not restart.
  display.state_ = EPaperState::RESET_END;
  EXPECT_FALSE(display.reset());
  EXPECT_EQ(display.reset_sub_, TestableInkplate13Spectra::RST_IO_WAIT);
  EXPECT_TRUE(display.pwr_en.level) << "panel power was not switched on";
}

}  // namespace esphome::epaper_spi::testing
