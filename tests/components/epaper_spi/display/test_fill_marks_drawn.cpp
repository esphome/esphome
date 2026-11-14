#include <gtest/gtest.h>

#include <memory>

#include "esphome/components/epaper_spi/epaper_spi_jd79660.h"
#include "esphome/components/epaper_spi/epaper_spi_t133a01.h"
#include "esphome/components/epaper_spi/epaper_waveshare_bwr.h"
#include "esphome/components/epaper_spi/epaper_weact_3c.h"

namespace esphome::epaper_spi::testing {
namespace {

/// Reaches the protected EPaperBase members of any driver, including final ones, through member pointers.
class EPaperBaseAccess : public EPaperBase {
 public:
  /// Leaves the display as a completed refresh leaves it: nothing drawn and no full update pending.
  static void set_refreshed(EPaperBase &display) {
    EXPECT_TRUE((display.*&EPaperBaseAccess::init_buffer_)(display.*&EPaperBaseAccess::buffer_length_));
    (display.*&EPaperBaseAccess::reset_bounds_)();
    display.*&EPaperBaseAccess::full_update_requested_ = false;
  }
  /// The request count is shared by the whole test binary, so a display left mid-update must give its request back.
  static void stop_fast_loop(EPaperBase &display) { (display.*&EPaperBaseAccess::high_frequency_loop_).stop(); }
};

template<typename Driver, typename... Args> std::unique_ptr<Driver> make_refreshed(Args... args) {
  auto display = std::make_unique<Driver>("test", 16, 2, nullptr, 0, args...);
  EPaperBaseAccess::set_refreshed(*display);
  return display;
}

template<typename Driver> std::unique_ptr<Driver> make_display() { return make_refreshed<Driver>(); }

template<> std::unique_ptr<EPaperJD79660> make_display() { return make_refreshed<EPaperJD79660>(nullptr, 0); }

/// Runs the UPDATE state; true if the update goes on to send the frame.
bool update_is_sent(EPaperBase &display) {
  display.update();
  display.loop();
  return display.is_updating();
}

/// fill() writes the whole buffer without drawing pixels, so it must mark the frame drawn itself. Otherwise the
/// UPDATE state finds nothing drawn after a refresh, and returns to idle without sending the frame.
template<typename Driver> class EPaperFillMarksDrawn : public ::testing::Test {
 protected:
  void TearDown() override { EPaperBaseAccess::stop_fast_loop(*this->display_); }
  std::unique_ptr<Driver> display_ = make_display<Driver>();
};

using Drivers = ::testing::Types<EPaperWaveshareBWR, EPaperT133A01, EPaperJD79660, EPaperWeAct3C>;
TYPED_TEST_SUITE(EPaperFillMarksDrawn, Drivers);

TYPED_TEST(EPaperFillMarksDrawn, FillOnlyUpdateIsSent) {
  this->display_->set_auto_clear(false);
  this->display_->fill(Color::WHITE);

  EXPECT_TRUE(update_is_sent(*this->display_)) << "fill-only update was skipped";
}

/// Auto clear fills the frame before the lambda runs, so an update whose lambda draws nothing still erases the
/// previous image.
TYPED_TEST(EPaperFillMarksDrawn, AutoClearUpdateIsSentWhenTheLambdaDrawsNothing) {
  this->display_->set_writer([](display::Display &) {});

  EXPECT_TRUE(update_is_sent(*this->display_)) << "auto clear did not erase the previous image";
}

}  // namespace
}  // namespace esphome::epaper_spi::testing
