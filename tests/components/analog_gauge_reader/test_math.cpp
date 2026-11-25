#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <vector>

// The reader is guarded by its USE_ macro, which is only set for ESP32 builds.
#define USE_ANALOG_GAUGE_READER
#include "esphome/components/analog_gauge_reader/analog_gauge_reader.h"

namespace esphome::analog_gauge_reader::testing {

namespace {

DialConfig make_dial(float min_angle, float max_angle, float min_value, float max_value) {
  DialConfig dial;
  dial.min_angle = min_angle;
  dial.max_angle = max_angle;
  dial.min_value = min_value;
  dial.max_value = max_value;
  return dial;
}

DialReading make_reading(const DialConfig *dial, float value) { return DialReading{dial, value, 0.0f, 1.0f}; }

}  // namespace

TEST(AngleMapping, ClockwiseFullCircle) {
  const DialConfig dial = make_dial(0.0f, 360.0f, 0.0f, 10.0f);
  // The detector reports 0 at East; the dial's 0 is North.
  EXPECT_NEAR(angle_to_value(-90.0f, dial), 0.0f, 1e-4f);
  EXPECT_NEAR(angle_to_value(0.0f, dial), 2.5f, 1e-4f);
  EXPECT_NEAR(angle_to_value(90.0f, dial), 5.0f, 1e-4f);
}

TEST(AngleMapping, Wrap360KeepsAnglesPositive) {
  EXPECT_NEAR(wrap360(-1.0f), 359.0f, 1e-4f);
  EXPECT_NEAR(wrap360(-370.0f), 350.0f, 1e-4f);
  EXPECT_NEAR(wrap360(0.0f), 0.0f, 1e-4f);
  EXPECT_NEAR(wrap360(720.0f), 0.0f, 1e-4f);
  EXPECT_NEAR(wrap360(12.5f), 12.5f, 1e-4f);
}

TEST(AngleMapping, WrapAroundRange) {
  const DialConfig dial = make_dial(300.0f, 60.0f, 0.0f, 10.0f);
  EXPECT_NEAR(angle_to_value(-90.0f, dial), 5.0f, 1e-4f);   // dial angle 0
  EXPECT_NEAR(angle_to_value(-30.0f, dial), 10.0f, 1e-4f);  // dial angle 60
  // The gap between the end and the start clamps to the nearer stop.
  EXPECT_NEAR(angle_to_value(30.0f, dial), 10.0f, 1e-4f);  // dial angle 120
}

TEST(AngleMapping, NeedleBeforeTheStartReadsTheMinimum) {
  // A 270 degree gauge: 225 -> 360 -> 135. At rest the needle sits just before
  // the zero mark, against the stop pin.
  const DialConfig dial = make_dial(225.0f, 135.0f, 0.0f, 10.0f);
  EXPECT_NEAR(angle_to_value(135.0f, dial), 0.0f, 1e-4f);  // dial angle 225, start
  EXPECT_NEAR(angle_to_value(-90.0f, dial), 5.0f, 1e-4f);  // dial angle 0, halfway
  EXPECT_NEAR(angle_to_value(45.0f, dial), 10.0f, 1e-4f);  // dial angle 135, end
  EXPECT_NEAR(angle_to_value(130.0f, dial), 0.0f, 1e-4f);  // dial angle 220, before the start
  EXPECT_NEAR(angle_to_value(50.0f, dial), 10.0f, 1e-4f);  // dial angle 140, past the end
}

TEST(AngleMapping, RangeStartingAtNorth) {
  // Zero at North, 270 degrees clockwise. The needle rests just before the zero
  // mark, which folds to 355 degrees.
  const DialConfig dial = make_dial(0.0f, 270.0f, 0.0f, 10.0f);
  EXPECT_NEAR(angle_to_value(265.0f, dial), 0.0f, 1e-4f);   // dial angle 355, before the start
  EXPECT_NEAR(angle_to_value(180.0f, dial), 10.0f, 1e-4f);  // dial angle 270, end
}

TEST(AngleMapping, RangeEndingAtNorth) {
  // 90 degrees clockwise to 360, which is North and folds to 0.
  const DialConfig dial = make_dial(90.0f, 360.0f, 0.0f, 10.0f);
  EXPECT_NEAR(angle_to_value(-90.0f, dial), 10.0f, 1e-4f);  // dial angle 0, full scale
  EXPECT_NEAR(angle_to_value(135.0f, dial), 5.0f, 1e-4f);   // dial angle 225, halfway
  EXPECT_NEAR(angle_to_value(-5.0f, dial), 0.0f, 1e-4f);    // dial angle 85, before the start
}

TEST(AngleMapping, CounterClockwise) {
  const DialConfig dial = make_dial(0.0f, 360.0f, 0.0f, 10.0f);
  DialConfig mirrored = dial;
  mirrored.clockwise = false;
  EXPECT_NEAR(angle_to_value(0.0f, mirrored), 7.5f, 1e-4f);
}

TEST(AngleMapping, OffsetAppliesBeforeTheMirror) {
  DialConfig dial = make_dial(0.0f, 180.0f, 0.0f, 10.0f);
  dial.clockwise = false;
  dial.angle_offset = 90.0f;
  // The offset moves the needle to the start of the range, where mirroring
  // leaves it; mirroring first would land on the end instead.
  EXPECT_NEAR(angle_to_value(0.0f, dial), 0.0f, 1e-4f);
}

TEST(AngleMapping, LargeOffsetAndNegativeAngleStayInRange) {
  // -170 + 90 - 300 folds to 340, which the calibration map must see as 340 and
  // not as a negative angle before its first point.
  DialConfig dial = make_dial(0.0f, 360.0f, 0.0f, 10.0f);
  dial.angle_offset = 300.0f;
  dial.calibration_mapping = {{0.0f, 100.0f}, {340.0f, 110.0f}};
  EXPECT_NEAR(angle_to_value(-170.0f, dial), 110.0f, 1e-4f);
}

TEST(AngleMapping, CalibrationWinsOverWrapAround) {
  DialConfig dial = make_dial(300.0f, 60.0f, 0.0f, 10.0f);
  dial.calibration_mapping = {{0.0f, 100.0f}, {360.0f, 200.0f}};
  EXPECT_NEAR(angle_to_value(-90.0f, dial), 100.0f, 1e-4f);
  EXPECT_NEAR(angle_to_value(0.0f, dial), 125.0f, 1e-4f);
}

TEST(AngleMapping, CalibrationClampsOutsideItsPoints) {
  DialConfig dial = make_dial(0.0f, 360.0f, 0.0f, 10.0f);
  dial.calibration_mapping = {{30.0f, 1.0f}, {60.0f, 2.0f}};
  EXPECT_NEAR(angle_to_value(-90.0f, dial), 1.0f, 1e-4f);  // dial angle 0
  EXPECT_NEAR(angle_to_value(0.0f, dial), 2.0f, 1e-4f);    // dial angle 90
}

TEST(StackedDigits, CarryRollsOverToTheLowerDial) {
  DialConfig fine_dial = make_dial(0.0f, 360.0f, 0.0f, 10.0f);
  fine_dial.scale = 1.0f;
  DialConfig coarse_dial = make_dial(0.0f, 360.0f, 0.0f, 10.0f);
  coarse_dial.scale = 10.0f;

  const DialReading readings[] = {make_reading(&fine_dial, 9.9f), make_reading(&coarse_dial, 0.99f)};
  int order[2];
  float phase[2];
  int digits[2];
  const float total = combine_stacked_digits(readings, 2, order, phase, digits);
  EXPECT_NEAR(total, 10.0f, 1e-4f);
  EXPECT_EQ(digits[0], 0);
  EXPECT_EQ(digits[1], 1);
}

TEST(StackedDigits, UsesTheFinestScaleForPrecision) {
  DialConfig fine_dial = make_dial(0.0f, 360.0f, 0.0f, 10.0f);
  fine_dial.scale = 0.1f;
  DialConfig coarse_dial = make_dial(0.0f, 360.0f, 0.0f, 10.0f);
  coarse_dial.scale = 1.0f;

  const DialReading readings[] = {make_reading(&fine_dial, 2.3f), make_reading(&coarse_dial, 1.23f)};
  int order[2];
  float phase[2];
  int digits[2];
  const float total = combine_stacked_digits(readings, 2, order, phase, digits);
  EXPECT_NEAR(total, 1.2f, 1e-4f);
  EXPECT_EQ(digits[0], 2);
  EXPECT_EQ(digits[1], 1);
}

TEST(StackedDigits, SortsDialsByScale) {
  DialConfig coarse_dial = make_dial(0.0f, 360.0f, 0.0f, 10.0f);
  coarse_dial.scale = 10.0f;
  DialConfig fine_dial = make_dial(0.0f, 360.0f, 0.0f, 10.0f);
  fine_dial.scale = 1.0f;

  // Configured coarse first: the result must not depend on the order.
  const DialReading readings[] = {make_reading(&coarse_dial, 0.99f), make_reading(&fine_dial, 9.9f)};
  int order[2];
  float phase[2];
  int digits[2];
  const float total = combine_stacked_digits(readings, 2, order, phase, digits);
  EXPECT_NEAR(total, 10.0f, 1e-4f);
  EXPECT_EQ(digits[0], 1);
  EXPECT_EQ(digits[1], 0);
}

TEST(AutoContrast, StretchesToTheFullRange) {
  std::vector<uint8_t> data = {50, 100, 150};
  apply_auto_contrast(data.data(), static_cast<int>(data.size()));
  EXPECT_EQ(0, data[0]);
  EXPECT_EQ(127, data[1]);
  EXPECT_EQ(255, data[2]);
}

TEST(AutoContrast, FlatCropIsLeftAlone) {
  std::vector<uint8_t> data = {80, 80, 80};
  apply_auto_contrast(data.data(), static_cast<int>(data.size()));
  EXPECT_EQ(80, data[0]);
  EXPECT_EQ(80, data[2]);
}

TEST(Contrast, ScalesAroundMidGreyAndClamps) {
  std::vector<uint8_t> data = {0, 100, 128, 156, 255};
  apply_contrast(data.data(), static_cast<int>(data.size()), 2.0f);
  EXPECT_EQ(0, data[0]);
  EXPECT_EQ(72, data[1]);
  EXPECT_EQ(128, data[2]);
  EXPECT_EQ(184, data[3]);
  EXPECT_EQ(255, data[4]);
}

TEST(Contrast, UnityMultiplierKeepsTheCrop) {
  std::vector<uint8_t> data = {10, 200};
  apply_contrast(data.data(), static_cast<int>(data.size()), 1.0f);
  EXPECT_EQ(10, data[0]);
  EXPECT_EQ(200, data[1]);
}

}  // namespace esphome::analog_gauge_reader::testing
