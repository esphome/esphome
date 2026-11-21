#include <gtest/gtest.h>

#include <cmath>

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

TEST(AngleMapping, WrapAroundRange) {
  const DialConfig dial = make_dial(300.0f, 60.0f, 0.0f, 10.0f);
  EXPECT_NEAR(angle_to_value(-90.0f, dial), 5.0f, 1e-4f);   // dial angle 0
  EXPECT_NEAR(angle_to_value(-30.0f, dial), 10.0f, 1e-4f);  // dial angle 60
  // An angle in the gap between the end and the start counts as past the end.
  EXPECT_NEAR(angle_to_value(30.0f, dial), 10.0f, 1e-4f);  // dial angle 120
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

}  // namespace esphome::analog_gauge_reader::testing
