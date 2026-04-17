// Regression tests for ESPTime::is_valid() optional checks.
//
// The RTC components (ds1307, bm8563, pcf85063, pcf8563, rx8130) read date/time
// fields from hardware but do not populate day_of_year. They call
// recalc_timestamp_utc(false) -- which skips day_of_year -- and then is_valid().
// These tests ensure the is_valid() overload can skip day_of_year validation so
// RTCs don't log "Invalid RTC time, not syncing to system clock." for valid times.

#include <gtest/gtest.h>
#include "esphome/core/time.h"

namespace esphome::testing {

// Build an ESPTime that mirrors what the RTC components construct: all fields
// populated from hardware except day_of_year (left zero-initialized).
static ESPTime make_rtc_like_time() {
  ESPTime t{};
  t.second = 30;
  t.minute = 15;
  t.hour = 12;
  t.day_of_week = 4;  // thursday
  t.day_of_month = 15;
  t.month = 4;
  t.year = 2026;
  // day_of_year intentionally left at 0 -- RTCs don't compute it.
  return t;
}

TEST(ESPTimeIsValid, DefaultRejectsZeroDayOfYear) {
  // Default is_valid() checks day_of_year; zero-init is out of range.
  ESPTime t = make_rtc_like_time();
  EXPECT_FALSE(t.is_valid());
}

TEST(ESPTimeIsValid, SkipDayOfYearAcceptsRTCLikeTime) {
  // RTC code path: skip day_of_year validation.
  ESPTime t = make_rtc_like_time();
  EXPECT_TRUE(t.is_valid(/*check_day_of_week=*/true, /*check_day_of_year=*/false));
}

TEST(ESPTimeIsValid, SkipDayOfYearStillRejectsOutOfRangeFields) {
  ESPTime t = make_rtc_like_time();
  t.hour = 25;
  EXPECT_FALSE(t.is_valid(/*check_day_of_week=*/true, /*check_day_of_year=*/false));
}

TEST(ESPTimeIsValid, SkipDayOfYearStillRejectsYearBefore2019) {
  ESPTime t = make_rtc_like_time();
  t.year = 2000;
  EXPECT_FALSE(t.is_valid(/*check_day_of_week=*/true, /*check_day_of_year=*/false));
}

TEST(ESPTimeIsValid, SkipBothDayChecksAcceptsGPSLikeTime) {
  // GPS path (gps_time.cpp) populates neither day_of_week nor day_of_year.
  ESPTime t{};
  t.second = 30;
  t.minute = 15;
  t.hour = 12;
  t.day_of_month = 15;
  t.month = 4;
  t.year = 2026;
  EXPECT_TRUE(t.is_valid(/*check_day_of_week=*/false, /*check_day_of_year=*/false));
  EXPECT_FALSE(t.is_valid());  // default still rejects
}

TEST(ESPTimeIsValid, FullyPopulatedAcceptsWithDefaults) {
  ESPTime t = make_rtc_like_time();
  t.day_of_year = 105;
  EXPECT_TRUE(t.is_valid());
}

}  // namespace esphome::testing
