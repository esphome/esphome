// ESPTime::from_epoch_utc() against the host C library's gmtime_r(), without a timezone configured.

#include <gtest/gtest.h>
#include <ctime>
#include <limits>
#include "esphome/core/time.h"

namespace esphome::time::testing {

static void expect_matches_gmtime(time_t epoch) {
  struct tm want{};
  ASSERT_NE(::gmtime_r(&epoch, &want), nullptr) << epoch;
  const ESPTime got = ESPTime::from_epoch_utc(epoch);
  EXPECT_EQ(got.year, want.tm_year + 1900) << epoch;
  EXPECT_EQ(got.month, want.tm_mon + 1) << epoch;
  EXPECT_EQ(got.day_of_month, want.tm_mday) << epoch;
  EXPECT_EQ(got.hour, want.tm_hour) << epoch;
  EXPECT_EQ(got.minute, want.tm_min) << epoch;
  EXPECT_EQ(got.second, want.tm_sec) << epoch;
  EXPECT_EQ(got.day_of_week, want.tm_wday + 1) << epoch;
  EXPECT_EQ(got.day_of_year, want.tm_yday + 1) << epoch;
  EXPECT_FALSE(got.is_dst) << epoch;
  EXPECT_EQ(got.timestamp, epoch) << epoch;
}

TEST(EpochUtc, MatchesGmtimeAtBoundaries) {
  for (time_t epoch : {
           static_cast<time_t>(0),
           static_cast<time_t>(-1),
           static_cast<time_t>(-86400),
           static_cast<time_t>(951782400),   // 2000-02-29, leap century
           static_cast<time_t>(951868799),   // 2000-02-29 23:59:59
           static_cast<time_t>(2147483647),  // 2038-01-19 03:14:07
           static_cast<time_t>(2147483648),
           static_cast<time_t>(4107542400),  // 2100-03-01, century without a leap day
           static_cast<time_t>(4107542399),  // 2100-02-28 23:59:59
       }) {
    expect_matches_gmtime(epoch);
  }
}

TEST(EpochUtc, MatchesGmtimeAcrossCenturies) {
  // An odd step (about 9.7 days plus some seconds) lands on every weekday, month and time of day
  constexpr time_t step = 837'461;
  for (time_t epoch = -2'208'988'800; epoch < 7'258'118'400; epoch += step) {  // 1900 to 2200
    expect_matches_gmtime(epoch);
  }
}

TEST(EpochUtc, MatchesGmtimeAcrossAllYears) {
  // About 246 years and some seconds per step, through every 400-year era from year 0 to 65535
  constexpr time_t step = 7'777'777'777;
  for (time_t epoch = -62'167'219'200; epoch < 2'005'949'145'600; epoch += step) {
    expect_matches_gmtime(epoch);
  }
}

TEST(EpochUtc, Years0To65535Convert) {
  expect_matches_gmtime(static_cast<time_t>(-62167219200));   // 0000-01-01 00:00:00
  expect_matches_gmtime(static_cast<time_t>(-62135596800));   // 0001-01-01 00:00:00
  expect_matches_gmtime(static_cast<time_t>(2005949145599));  // 65535-12-31 23:59:59
}

TEST(EpochUtc, RecalcTimestampRoundTrips) {
  // Both ways of naming the day, including dates before 1970
  constexpr time_t step = 837'461;
  for (time_t epoch = -2'208'988'800; epoch < 7'258'118'400; epoch += step) {
    ESPTime by_day_of_year = ESPTime::from_epoch_utc(epoch);
    by_day_of_year.recalc_timestamp_utc(true);
    EXPECT_EQ(by_day_of_year.timestamp, epoch);
    ESPTime by_month = ESPTime::from_epoch_utc(epoch);
    by_month.recalc_timestamp_utc(false);
    EXPECT_EQ(by_month.timestamp, epoch);
  }
}

TEST(EpochUtc, RecalcTimestampAtThe32BitMinimum) {
  // 1901-12-13 20:45:52, the smallest 32-bit time_t
  ESPTime t = ESPTime::from_epoch_utc(static_cast<time_t>(-2147483648LL));
  t.recalc_timestamp_utc(false);
  EXPECT_EQ(t.timestamp, static_cast<time_t>(-2147483648LL));
}

TEST(EpochUtc, EpochsOutsideSixteenBitYearsAreInvalid) {
  for (time_t epoch : {
           static_cast<time_t>(-62167219201),
           static_cast<time_t>(2005949145600),
           std::numeric_limits<time_t>::min(),
           std::numeric_limits<time_t>::max(),
       }) {
    const ESPTime got = ESPTime::from_epoch_utc(epoch);
    EXPECT_FALSE(got.is_valid()) << epoch;
    EXPECT_EQ(got.year, 0) << epoch;
  }
}

}  // namespace esphome::time::testing
