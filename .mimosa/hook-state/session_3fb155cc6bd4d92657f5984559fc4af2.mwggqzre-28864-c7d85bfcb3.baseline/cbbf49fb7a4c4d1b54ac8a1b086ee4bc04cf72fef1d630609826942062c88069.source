// Year roll-over in ESPTime::increment_second() and increment_day(): 2100 is a common year, 2000 and 2024 are leap
// years, so the day of the year must wrap after 365 or 366 days accordingly.

#include <gtest/gtest.h>
#include "esphome/core/time.h"

namespace esphome::testing {

static ESPTime last_day_of(uint16_t year, uint16_t day_of_year) {
  ESPTime t{};
  t.second = 59;
  t.minute = 59;
  t.hour = 23;
  t.day_of_week = 5;
  t.day_of_month = 31;
  t.day_of_year = day_of_year;
  t.month = 12;
  t.year = year;
  return t;
}

static void expect_new_year(const ESPTime &t, uint16_t year) {
  EXPECT_EQ(t.year, year);
  EXPECT_EQ(t.month, 1);
  EXPECT_EQ(t.day_of_month, 1);
  EXPECT_EQ(t.day_of_year, 1);
}

TEST(ESPTimeIncrement, DayRollsOverCommonCenturyYear) {
  ESPTime t = last_day_of(2100, 365);
  t.increment_day();
  expect_new_year(t, 2101);
}

TEST(ESPTimeIncrement, SecondRollsOverCommonCenturyYear) {
  ESPTime t = last_day_of(2100, 365);
  t.increment_second();
  expect_new_year(t, 2101);
  EXPECT_EQ(t.hour, 0);
  EXPECT_EQ(t.minute, 0);
  EXPECT_EQ(t.second, 0);
}

TEST(ESPTimeIncrement, DayRollsOverLeapYears) {
  ESPTime t = last_day_of(2000, 366);
  t.increment_day();
  expect_new_year(t, 2001);
  t = last_day_of(2024, 366);
  t.increment_day();
  expect_new_year(t, 2025);
}

TEST(ESPTimeIncrement, DayStaysInLeapYearOnDay365) {
  ESPTime t = last_day_of(2024, 365);
  t.day_of_month = 30;
  t.increment_day();
  EXPECT_EQ(t.year, 2024);
  EXPECT_EQ(t.day_of_year, 366);
  EXPECT_EQ(t.day_of_month, 31);
}

}  // namespace esphome::testing
