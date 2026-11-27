#include <gtest/gtest.h>

#include <ctime>

#include "esphome/core/time_utc.h"

namespace esphome::core::testing {

using namespace esphome::internal;

TEST(TimeUtc, MinValidEpochIsStartOfMinValidYear) {
  EXPECT_EQ(static_cast<time_t>(days_to_year_start(MIN_VALID_YEAR)) * SECONDS_PER_DAY, MIN_VALID_EPOCH);
}

TEST(TimeUtc, DaysBeforeMonth) {
  static constexpr int NON_LEAP[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
  for (int year : {1900, 2000, 2023, 2024}) {
    for (int month = 1; month <= 12; month++) {
      const int leap_day = is_leap_year(year) && month > 2;
      EXPECT_EQ(days_before_month(year, month), NON_LEAP[month - 1] + leap_day) << year << "-" << month;
    }
  }
}

TEST(TimeUtc, WeekdayMatchesGmtime) {
  for (int32_t days = -800; days <= 800; days++) {
    const time_t epoch = static_cast<time_t>(days) * SECONDS_PER_DAY;
    struct tm want{};
    ASSERT_NE(::gmtime_r(&epoch, &want), nullptr);
    EXPECT_EQ(days_to_weekday(days), want.tm_wday) << days;
  }
}

}  // namespace esphome::core::testing
