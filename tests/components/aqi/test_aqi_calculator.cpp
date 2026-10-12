#include <gtest/gtest.h>

#include "esphome/components/aqi/aqi_calculator.h"
#include "esphome/components/aqi/caqi_calculator.h"

namespace esphome::aqi::testing {

// US AQI (EPA 2024): PM2.5 225.5-500.4 -> 301-500, PM10 425-604 -> 301-500.

TEST(USAQI, LowRangeUnaffectedByExtendedFlag) {
  AQICalculator calc;
  // PM2.5 25 drives over PM10 50; well below the top band, so the flag changes nothing.
  EXPECT_EQ(calc.get_aqi(25.0f, 50.0f, false), 81);
  EXPECT_EQ(calc.get_aqi(25.0f, 50.0f, true), 81);
}

TEST(USAQI, HazardousInterpolatesNotPinnedAt301) {
  AQICalculator calc;
  // Regression guard: the old FLT_MAX top bucket collapsed every hazardous reading to 301.
  EXPECT_EQ(calc.get_aqi(225.5f, 0.0f, false), 301);  // band start
  EXPECT_EQ(calc.get_aqi(250.0f, 0.0f, false), 319);  // interpolated, not 301
  EXPECT_EQ(calc.get_aqi(500.4f, 0.0f, false), 500);  // band top
}

TEST(USAQI, DefaultClampsAtStandardMaximum) {
  AQICalculator calc;
  EXPECT_EQ(calc.get_aqi(600.0f, 0.0f, false), 500);
  EXPECT_EQ(calc.get_aqi(1000.0f, 0.0f, false), 500);
  EXPECT_EQ(calc.get_aqi(0.0f, 604.0f, false), 500);  // PM10 top breakpoint
}

TEST(USAQI, ExtendedRangeExtrapolatesBeyond500) {
  AQICalculator calc;
  EXPECT_EQ(calc.get_aqi(600.0f, 0.0f, true), 572);
  EXPECT_EQ(calc.get_aqi(1000.0f, 0.0f, true), 862);
  EXPECT_EQ(calc.get_aqi(0.0f, 700.0f, true), 607);  // PM10 extrapolated past 500
}

TEST(USAQI, ExtendedRangeSaturatesUint16NoWraparound) {
  AQICalculator calc;
  // An absurd concentration would overflow uint16_t; it must saturate, not wrap to a small value.
  EXPECT_EQ(calc.get_aqi(100000.0f, 0.0f, true), 65535);
}

TEST(USAQI, WorseOfTwoPollutantsWins) {
  AQICalculator calc;
  // PM10 604 -> 500 dominates PM2.5 25 -> 81.
  EXPECT_EQ(calc.get_aqi(25.0f, 604.0f, false), 500);
}

// CAQI (CITEAIR): no maximum by spec -- the top ">100" class is open, so it is always unbounded
// and the extended_range flag does not apply.

TEST(CAQI, LowRange) {
  CAQICalculator calc;
  EXPECT_EQ(calc.get_aqi(25.0f, 50.0f, false), 50);
}

TEST(CAQI, ContinuousAt100NoPinAt101) {
  CAQICalculator calc;
  // Old code pinned everything above the top breakpoint to 101; now it reaches exactly 100.
  EXPECT_EQ(calc.get_aqi(110.1f, 0.0f, false), 100);
}

TEST(CAQI, UnboundedAboveTopBand) {
  CAQICalculator calc;
  EXPECT_EQ(calc.get_aqi(200.0f, 0.0f, false), 139);
  EXPECT_EQ(calc.get_aqi(2000.0f, 0.0f, false), 925);
}

TEST(CAQI, ExtendedRangeFlagIsIgnored) {
  CAQICalculator calc;
  // CAQI is always unbounded, so the flag must make no difference either way.
  EXPECT_EQ(calc.get_aqi(200.0f, 0.0f, true), calc.get_aqi(200.0f, 0.0f, false));
  EXPECT_EQ(calc.get_aqi(2000.0f, 0.0f, true), calc.get_aqi(2000.0f, 0.0f, false));
}

TEST(CAQI, SaturatesUint16NoWraparound) {
  CAQICalculator calc;
  // CAQI is unbounded, so an extreme reading can extrapolate past uint16_t; it must saturate,
  // not wrap around to a small (falsely "good") value.
  EXPECT_EQ(calc.get_aqi(200000.0f, 0.0f, false), 65535);
}

}  // namespace esphome::aqi::testing
