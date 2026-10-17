#include <gtest/gtest.h>

#include "../common.h"
#include "esphome/components/epaper_spi/epaper_spi_inkplate13spectra.h"
#include "esphome/core/application.h"

namespace esphome::epaper_spi::testing {

/// Exposes the power-up state machine in reset() so it can be driven directly.
class TestableInkplate13Spectra : public EPaperInkplate13Spectra {
 public:
  TestableInkplate13Spectra() : EPaperInkplate13Spectra("test", 8, 2, nullptr, 0, false) {}

  void install(spi::SPIDelegate *delegate = nullptr) {
    this->delegate_ = delegate;
    this->set_dc_pin(&this->dc);
    this->set_cs_pins(&this->cs, &this->cs1);
    this->set_busy_pin(&this->busy);
    this->set_rst_pin(&this->rst);
    this->set_pwr_en_pin(&this->pwr_en);
    this->set_bs_pins(&this->bs0, &this->bs1);
  }

  using EPaperInkplate13Spectra::check_refresh_ran_;
  using EPaperInkplate13Spectra::refresh_screen;
  using EPaperInkplate13Spectra::refresh_started_ms_;
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

/// When the panel ignores the refresh command (for example because it never powered up), the
/// busy pin is released almost at once. That must be flagged rather than reported as a normal
/// update, which is what happened in the field: the log showed a successful update while the
/// screen never changed.
TEST(EPaperInkplate13Spectra, RefreshThatEndsAtOnceIsFlagged) {
  TestableInkplate13Spectra display;
  RecordingDelegate bus(&display.dc);
  display.install(&bus);

  display.refresh_screen(false);  // the busy pin is released in the same loop pass
  EXPECT_FALSE(display.check_refresh_ran_()) << "an instant refresh was accepted as real";
}

TEST(EPaperInkplate13Spectra, RefreshThatKeepsThePanelBusyIsAccepted) {
  TestableInkplate13Spectra display;
  RecordingDelegate bus(&display.dc);
  display.install(&bus);

  display.refresh_screen(false);
  // Pretend the refresh command went out 20 s ago, as long as a real refresh takes.
  display.refresh_started_ms_ = App.get_loop_component_start_time() - 20000;
  EXPECT_TRUE(display.check_refresh_ran_());
}

/// A partial update of a rectangle that is entirely off screen has nothing to refresh, so it must not start a
/// cycle: no panel power-up and no refresh. One that is on screen does start a cycle.
TEST(EPaperInkplate13Spectra, OffScreenPartialUpdateDoesNothing) {
  TestableInkplate13Spectra display;
  display.install();

  display.display_partial(-100, -100, 10, 10);
  EXPECT_EQ(display.state_, EPaperState::IDLE) << "an off-screen partial update started a refresh cycle";

  display.display_partial(0, 0, 4, 2);
  EXPECT_EQ(display.state_, EPaperState::RESET) << "an on-screen partial update did not start a refresh cycle";
}

/// power_off() also runs when an update is abandoned before the refresh command was sent; there
/// is nothing to check then, so no warning may be raised.
TEST(EPaperInkplate13Spectra, NoRefreshCommandMeansNothingToCheck) {
  TestableInkplate13Spectra display;
  display.install();

  EXPECT_TRUE(display.check_refresh_ran_());
}

}  // namespace esphome::epaper_spi::testing
