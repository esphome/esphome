#include <gtest/gtest.h>

#include <memory>

#include "esphome/components/epaper_spi/epaper_spi_jd79660.h"
#include "esphome/components/epaper_spi/epaper_spi_t133a01.h"
#include "esphome/components/epaper_spi/epaper_waveshare_bwr.h"
#include "esphome/components/epaper_spi/epaper_weact_3c.h"

namespace esphome::epaper_spi::testing {
namespace {

/// A driver left as a completed refresh leaves it: nothing drawn and no full update pending.
template<typename Driver> class TestableDriver : public Driver {
 public:
  template<typename... Args> TestableDriver(Args... args) : Driver("test", 16, 2, nullptr, 0, args...) {
    EXPECT_TRUE(this->init_buffer_(this->buffer_length_));
    this->reset_bounds_();
    this->full_update_requested_ = false;
  }

  /// Runs the UPDATE state; true if the update goes on to send the frame.
  bool update_is_sent() {
    this->update();
    this->loop();
    return this->is_updating();
  }
};

template<typename Driver> std::unique_ptr<TestableDriver<Driver>> make_display() {
  return std::make_unique<TestableDriver<Driver>>();
}

template<> std::unique_ptr<TestableDriver<EPaperJD79660>> make_display() {
  return std::make_unique<TestableDriver<EPaperJD79660>>(nullptr, 0);
}

/// fill() writes the whole buffer without drawing pixels, so it must mark the frame drawn itself. Otherwise the
/// UPDATE state finds nothing drawn after a refresh, and returns to idle without sending the frame.
template<typename Driver> class EPaperFillMarksDrawn : public ::testing::Test {
 protected:
  std::unique_ptr<TestableDriver<Driver>> display_ = make_display<Driver>();
};

using Drivers = ::testing::Types<EPaperWaveshareBWR, EPaperT133A01, EPaperJD79660, EPaperWeAct3C>;
TYPED_TEST_SUITE(EPaperFillMarksDrawn, Drivers);

TYPED_TEST(EPaperFillMarksDrawn, FillOnlyUpdateIsSent) {
  this->display_->set_auto_clear(false);
  this->display_->fill(Color::WHITE);

  EXPECT_TRUE(this->display_->update_is_sent()) << "fill-only update was skipped";
}

/// Auto clear fills the frame before the lambda runs, so an update whose lambda draws nothing still erases the
/// previous image.
TYPED_TEST(EPaperFillMarksDrawn, AutoClearUpdateIsSentWhenTheLambdaDrawsNothing) {
  this->display_->set_writer([](display::Display &) {});

  EXPECT_TRUE(this->display_->update_is_sent()) << "auto clear did not erase the previous image";
}

}  // namespace
}  // namespace esphome::epaper_spi::testing
