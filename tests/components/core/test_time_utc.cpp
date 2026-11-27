#include <gtest/gtest.h>

#include <ctime>

#include "esphome/core/time_utc.h"

namespace esphome::core::testing {

using namespace esphome::internal;

static_assert(days_to_weekday(0) == 4);   // 1970-01-01, Thursday
static_assert(days_to_weekday(3) == 0);   // 1970-01-04, Sunday
static_assert(days_to_weekday(-1) == 3);  // 1969-12-31, Wednesday
static_assert(days_to_weekday(-4) == 0);
static_assert(days_to_weekday(-7) == 4);

TEST(TimeUtc, MinValidEpochIsStartOfMinValidYear) {
  EXPECT_EQ(static_cast<time_t>(days_to_year_start(MIN_VALID_YEAR)) * SECONDS_PER_DAY, MIN_VALID_EPOCH);
}

TEST(TimeUtc, DaysBeforeMonth) {
  static constexpr int NON_LEAP[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
  for (int month = 1; month <= 12; month++) {
    const int leap_day = month > 2;
    EXPECT_EQ(days_before_month(2023, month), NON_LEAP[month - 1]) << month;
    EXPECT_EQ(days_before_month(1900, month), NON_LEAP[month - 1]) << month;
    EXPECT_EQ(days_before_month(2024, month), NON_LEAP[month - 1] + leap_day) << month;
    EXPECT_EQ(days_before_month(2000, month), NON_LEAP[month - 1] + leap_day) << month;
  }
}

TEST(TimeUtc, YdayToMonthDayBoundaries) {
  struct Case {
    int year;
    int yday;
    uint8_t month;
    uint8_t day;
  };
  for (const Case &c : {
           Case{2023, 0, 1, 1},
           Case{2023, 30, 1, 31},
           Case{2023, 31, 2, 1},
           Case{2023, 58, 2, 28},
           Case{2023, 59, 3, 1},
           Case{2024, 59, 2, 29},
           Case{2024, 60, 3, 1},
           Case{1900, 59, 3, 1},
           Case{2000, 59, 2, 29},
           Case{2023, 364, 12, 31},
           Case{2024, 365, 12, 31},
       }) {
    const MonthDay md = yday_to_month_day(c.year, c.yday);
    EXPECT_EQ(md.month, c.month) << c.year << " " << c.yday;
    EXPECT_EQ(md.day, c.day) << c.year << " " << c.yday;
  }
}

TEST(TimeUtc, YdayRoundTripsThroughDaysBeforeMonth) {
  for (int year : {1900, 2000, 2023, 2024}) {
    const int days = is_leap_year(year) ? 366 : 365;
    for (int yday = 0; yday < days; yday++) {
      const MonthDay md = yday_to_month_day(year, yday);
      ASSERT_GE(md.day, 1);
      ASSERT_LE(md.day, days_in_month(year, md.month));
      EXPECT_EQ(days_before_month(year, md.month) + md.day - 1, yday) << year;
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
