#include <gtest/gtest.h>

#include <memory>

#include "../common.h"

namespace esphome::cst816::testing {

class CST816TouchscreenTest : public ::testing::Test {
 protected:
  void feed_(const std::array<uint8_t, FRAME_SIZE> &frame) {
    this->bus_ = std::make_unique<FrameBus>(frame);
    this->touchscreen_.set_i2c_bus(this->bus_.get());
    this->touchscreen_.set_i2c_address(ADDRESS);
    this->touchscreen_.update_touches();
  }

  std::unique_ptr<FrameBus> bus_;
  CST816Touchscreen touchscreen_;
};

// An idle frame reports 0xFF in the touch count byte and 0xFFF coordinates. Masking 0xFF to the
// low two bits turns that into three touches at the maximum position, a phantom tap.
TEST_F(CST816TouchscreenTest, IdleAllOnesFrameIsIgnored) {
  this->feed_(make_frame(0xFF, 0x0FFF, 0x0FFF));
  EXPECT_TRUE(this->touchscreen_.get_touches().empty());
}

TEST_F(CST816TouchscreenTest, ZeroTouchCountIsIgnored) {
  this->feed_(make_frame(0));
  EXPECT_TRUE(this->touchscreen_.get_touches().empty());
}

// The low nibble is the count, so 0x05 must not be mistaken for a single touch by masking with 0x03.
TEST_F(CST816TouchscreenTest, TouchCountAboveThreeIsIgnored) {
  this->feed_(make_frame(0x05, 100, 200));
  EXPECT_TRUE(this->touchscreen_.get_touches().empty());
}

TEST_F(CST816TouchscreenTest, SingleTouchIsReported) {
  this->feed_(make_frame(1, 300, 100));
  auto touches = this->touchscreen_.get_touches();
  ASSERT_EQ(touches.size(), 1u);
  EXPECT_EQ(touches[0].x_raw, 300);
  EXPECT_EQ(touches[0].y_raw, 100);
}

// CST826/CST836 can report two points; the driver only consumes the first one.
TEST_F(CST816TouchscreenTest, TwoPointFrameReportsFirstPoint) {
  this->feed_(make_frame(2, 300, 100));
  auto touches = this->touchscreen_.get_touches();
  ASSERT_EQ(touches.size(), 1u);
  EXPECT_EQ(touches[0].x_raw, 300);
  EXPECT_EQ(touches[0].y_raw, 100);
}

TEST_F(CST816TouchscreenTest, SentinelPositionIsIgnored) {
  this->feed_(make_frame(1, 0x0FFF, 100));
  EXPECT_TRUE(this->touchscreen_.get_touches().empty());
}

// x_raw_max_/y_raw_max_ are calibration endpoints, not validity limits: the base class applies them
// after swapping axes and clamps readings outside them. A touch beyond the calibrated maximum is
// still a real touch and must reach the base class.
TEST_F(CST816TouchscreenTest, CalibrationBoundsDoNotDropTouches) {
  this->touchscreen_.set_calibration(0, 240, 0, 320);
  this->touchscreen_.set_swap_xy(true);
  this->feed_(make_frame(1, 300, 100));
  auto touches = this->touchscreen_.get_touches();
  ASSERT_EQ(touches.size(), 1u);
  EXPECT_EQ(touches[0].x_raw, 100);
  EXPECT_EQ(touches[0].y_raw, 300);
}

}  // namespace esphome::cst816::testing
