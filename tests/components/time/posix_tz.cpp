// Tests for time conversion functions, DST detection, and ESPTime::strptime.
//
// These tests cover the permanent timezone functions: epoch_to_local_tm, is_in_dst,
// calculate_dst_transition, and the internal helper functions they depend on.

// Enable USE_TIME_TIMEZONE for tests
#define USE_TIME_TIMEZONE

#include <gtest/gtest.h>
#include <cstdlib>
#include <ctime>
#include <string>
#include <tuple>
#include "esphome/components/time/posix_tz.h"
#include "esphome/core/time.h"

namespace esphome::time::testing {

// Helper to create UTC epoch from date/time components (for test readability)
static time_t make_utc(int year, int month, int day, int hour = 0, int min = 0, int sec = 0) {
  int64_t days = 0;
  for (int y = 1970; y < year; y++) {
    days += (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? 366 : 365;
  }
  static const int DAYS_BEFORE[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
  days += DAYS_BEFORE[month - 1];
  if (month > 2 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)))
    days++;  // Leap year adjustment
  days += day - 1;
  return days * 86400 + hour * 3600 + min * 60 + sec;
}

// Helper to build a US Eastern timezone (EST5EDT,M3.2.0/2,M11.1.0/2)
static ParsedTimezone make_us_eastern() {
  ParsedTimezone tz{};
  tz.std_offset_seconds = 5 * 3600;
  tz.dst_offset_seconds = 4 * 3600;
  tz.dst_start.type = DSTRuleType::MONTH_WEEK_DAY;
  tz.dst_start.month = 3;
  tz.dst_start.week = 2;
  tz.dst_start.day_of_week = 0;
  tz.dst_start.time_seconds = 2 * 3600;
  tz.dst_end.type = DSTRuleType::MONTH_WEEK_DAY;
  tz.dst_end.month = 11;
  tz.dst_end.week = 1;
  tz.dst_end.day_of_week = 0;
  tz.dst_end.time_seconds = 2 * 3600;
  return tz;
}

// Helper to build a US Central timezone (CST6CDT,M3.2.0,M11.1.0)
static ParsedTimezone make_us_central() {
  ParsedTimezone tz{};
  tz.std_offset_seconds = 6 * 3600;
  tz.dst_offset_seconds = 5 * 3600;
  tz.dst_start.type = DSTRuleType::MONTH_WEEK_DAY;
  tz.dst_start.month = 3;
  tz.dst_start.week = 2;
  tz.dst_start.day_of_week = 0;
  tz.dst_start.time_seconds = 2 * 3600;
  tz.dst_end.type = DSTRuleType::MONTH_WEEK_DAY;
  tz.dst_end.month = 11;
  tz.dst_end.week = 1;
  tz.dst_end.day_of_week = 0;
  tz.dst_end.time_seconds = 2 * 3600;
  return tz;
}

// Helper to build New Zealand timezone (NZST-12NZDT,M9.5.0,M4.1.0/3)
static ParsedTimezone make_new_zealand() {
  ParsedTimezone tz{};
  tz.std_offset_seconds = -12 * 3600;
  tz.dst_offset_seconds = -13 * 3600;
  tz.dst_start.type = DSTRuleType::MONTH_WEEK_DAY;
  tz.dst_start.month = 9;
  tz.dst_start.week = 5;
  tz.dst_start.day_of_week = 0;
  tz.dst_start.time_seconds = 2 * 3600;
  tz.dst_end.type = DSTRuleType::MONTH_WEEK_DAY;
  tz.dst_end.month = 4;
  tz.dst_end.week = 1;
  tz.dst_end.day_of_week = 0;
  tz.dst_end.time_seconds = 3 * 3600;
  return tz;
}

// Helper to build Australia/Sydney timezone (AEST-10AEDT,M10.1.0,M4.1.0/3)
static ParsedTimezone make_australia_sydney() {
  ParsedTimezone tz{};
  tz.std_offset_seconds = -10 * 3600;
  tz.dst_offset_seconds = -11 * 3600;
  tz.dst_start.type = DSTRuleType::MONTH_WEEK_DAY;
  tz.dst_start.month = 10;
  tz.dst_start.week = 1;
  tz.dst_start.day_of_week = 0;
  tz.dst_start.time_seconds = 2 * 3600;
  tz.dst_end.type = DSTRuleType::MONTH_WEEK_DAY;
  tz.dst_end.month = 4;
  tz.dst_end.week = 1;
  tz.dst_end.day_of_week = 0;
  tz.dst_end.time_seconds = 3 * 3600;
  return tz;
}

// ============================================================================
// Helper function tests
// ============================================================================

TEST(PosixTz, JulianDay60IsMarch1) {
  // J60 is always March 1 (J format ignores leap years by design)
  int month, day;
  internal::julian_to_month_day(60, month, day);
  EXPECT_EQ(month, 3);
  EXPECT_EQ(day, 1);
}

TEST(PosixTz, DayOfYear59DiffersByLeap) {
  int month, day;
  // Day 59 in leap year is Feb 29
  internal::day_of_year_to_month_day(59, 2024, month, day);
  EXPECT_EQ(month, 2);
  EXPECT_EQ(day, 29);
  // Day 59 in non-leap year is March 1
  internal::day_of_year_to_month_day(59, 2025, month, day);
  EXPECT_EQ(month, 3);
  EXPECT_EQ(day, 1);
}

TEST(PosixTz, DayOfWeekKnownDates) {
  // January 1, 1970 was Thursday (4)
  EXPECT_EQ(internal::day_of_week(1970, 1, 1), 4);
  // January 1, 2000 was Saturday (6)
  EXPECT_EQ(internal::day_of_week(2000, 1, 1), 6);
  // March 8, 2026 is Sunday (0) - US DST start
  EXPECT_EQ(internal::day_of_week(2026, 3, 8), 0);
}

TEST(PosixTz, LeapYearDetection) {
  EXPECT_FALSE(internal::is_leap_year(1900));  // Divisible by 100 but not 400
  EXPECT_TRUE(internal::is_leap_year(2000));   // Divisible by 400
  EXPECT_TRUE(internal::is_leap_year(2024));   // Divisible by 4
  EXPECT_FALSE(internal::is_leap_year(2025));  // Not divisible by 4
}

TEST(PosixTz, JulianDay1IsJan1) {
  int month, day;
  internal::julian_to_month_day(1, month, day);
  EXPECT_EQ(month, 1);
  EXPECT_EQ(day, 1);
}

TEST(PosixTz, JulianDay31IsJan31) {
  int month, day;
  internal::julian_to_month_day(31, month, day);
  EXPECT_EQ(month, 1);
  EXPECT_EQ(day, 31);
}

TEST(PosixTz, JulianDay32IsFeb1) {
  int month, day;
  internal::julian_to_month_day(32, month, day);
  EXPECT_EQ(month, 2);
  EXPECT_EQ(day, 1);
}

TEST(PosixTz, JulianDay59IsFeb28) {
  int month, day;
  internal::julian_to_month_day(59, month, day);
  EXPECT_EQ(month, 2);
  EXPECT_EQ(day, 28);
}

TEST(PosixTz, JulianDay365IsDec31) {
  int month, day;
  internal::julian_to_month_day(365, month, day);
  EXPECT_EQ(month, 12);
  EXPECT_EQ(day, 31);
}

TEST(PosixTz, DayOfYear0IsJan1) {
  int month, day;
  internal::day_of_year_to_month_day(0, 2025, month, day);
  EXPECT_EQ(month, 1);
  EXPECT_EQ(day, 1);
}

TEST(PosixTz, DaysInMonthRegular) {
  // Test all 12 months to ensure switch coverage
  EXPECT_EQ(internal::days_in_month(2025, 1), 31);   // Jan - default case
  EXPECT_EQ(internal::days_in_month(2025, 2), 28);   // Feb - case 2
  EXPECT_EQ(internal::days_in_month(2025, 3), 31);   // Mar - default case
  EXPECT_EQ(internal::days_in_month(2025, 4), 30);   // Apr - case 4
  EXPECT_EQ(internal::days_in_month(2025, 5), 31);   // May - default case
  EXPECT_EQ(internal::days_in_month(2025, 6), 30);   // Jun - case 6
  EXPECT_EQ(internal::days_in_month(2025, 7), 31);   // Jul - default case
  EXPECT_EQ(internal::days_in_month(2025, 8), 31);   // Aug - default case
  EXPECT_EQ(internal::days_in_month(2025, 9), 30);   // Sep - case 9
  EXPECT_EQ(internal::days_in_month(2025, 10), 31);  // Oct - default case
  EXPECT_EQ(internal::days_in_month(2025, 11), 30);  // Nov - case 11
  EXPECT_EQ(internal::days_in_month(2025, 12), 31);  // Dec - default case
}

TEST(PosixTz, DaysInMonthLeapYear) {
  EXPECT_EQ(internal::days_in_month(2024, 2), 29);
  EXPECT_EQ(internal::days_in_month(2025, 2), 28);
}

TEST(PosixTz, PlainDay365LeapYear) {
  int month, day;
  internal::day_of_year_to_month_day(365, 2024, month, day);
  EXPECT_EQ(month, 12);
  EXPECT_EQ(day, 31);
}

TEST(PosixTz, PlainDay364NonLeapYear) {
  int month, day;
  internal::day_of_year_to_month_day(364, 2025, month, day);
  EXPECT_EQ(month, 12);
  EXPECT_EQ(day, 31);
}

// ============================================================================
// DST transition calculation tests
// ============================================================================

TEST(PosixTz, DstStartUSEastern2026) {
  // March 8, 2026 is 2nd Sunday of March
  auto tz = make_us_eastern();

  time_t dst_start = internal::calculate_dst_transition(2026, tz.dst_start, tz.std_offset_seconds);
  struct tm tm;
  internal::epoch_to_tm_utc(dst_start, &tm);

  // At 2:00 AM EST (UTC-5), so 7:00 AM UTC
  EXPECT_EQ(tm.tm_year + 1900, 2026);
  EXPECT_EQ(tm.tm_mon + 1, 3);  // March
  EXPECT_EQ(tm.tm_mday, 8);     // 8th
  EXPECT_EQ(tm.tm_hour, 7);     // 7:00 UTC = 2:00 EST
}

TEST(PosixTz, DstEndUSEastern2026) {
  // November 1, 2026 is 1st Sunday of November
  auto tz = make_us_eastern();

  time_t dst_end = internal::calculate_dst_transition(2026, tz.dst_end, tz.dst_offset_seconds);
  struct tm tm;
  internal::epoch_to_tm_utc(dst_end, &tm);

  // At 2:00 AM EDT (UTC-4), so 6:00 AM UTC
  EXPECT_EQ(tm.tm_year + 1900, 2026);
  EXPECT_EQ(tm.tm_mon + 1, 11);  // November
  EXPECT_EQ(tm.tm_mday, 1);      // 1st
  EXPECT_EQ(tm.tm_hour, 6);      // 6:00 UTC = 2:00 EDT
}

TEST(PosixTz, LastSundayOfMarch2026) {
  // Europe: M3.5.0 = last Sunday of March = March 29, 2026
  DSTRule rule{};
  rule.type = DSTRuleType::MONTH_WEEK_DAY;
  rule.month = 3;
  rule.week = 5;
  rule.day_of_week = 0;
  rule.time_seconds = 2 * 3600;
  time_t transition = internal::calculate_dst_transition(2026, rule, 0);
  struct tm tm;
  internal::epoch_to_tm_utc(transition, &tm);
  EXPECT_EQ(tm.tm_mday, 29);
  EXPECT_EQ(tm.tm_wday, 0);  // Sunday
}

TEST(PosixTz, LastSundayOfOctober2026) {
  // Europe: M10.5.0 = last Sunday of October = October 25, 2026
  DSTRule rule{};
  rule.type = DSTRuleType::MONTH_WEEK_DAY;
  rule.month = 10;
  rule.week = 5;
  rule.day_of_week = 0;
  rule.time_seconds = 3 * 3600;
  time_t transition = internal::calculate_dst_transition(2026, rule, 0);
  struct tm tm;
  internal::epoch_to_tm_utc(transition, &tm);
  EXPECT_EQ(tm.tm_mday, 25);
  EXPECT_EQ(tm.tm_wday, 0);  // Sunday
}

TEST(PosixTz, FirstSundayOfApril2026) {
  // April 5, 2026 is 1st Sunday
  DSTRule rule{};
  rule.type = DSTRuleType::MONTH_WEEK_DAY;
  rule.month = 4;
  rule.week = 1;
  rule.day_of_week = 0;
  rule.time_seconds = 0;
  time_t transition = internal::calculate_dst_transition(2026, rule, 0);
  struct tm tm;
  internal::epoch_to_tm_utc(transition, &tm);
  EXPECT_EQ(tm.tm_mday, 5);
  EXPECT_EQ(tm.tm_wday, 0);
}

// ============================================================================
// DST detection tests
// ============================================================================

TEST(PosixTz, IsInDstUSEasternSummer) {
  auto tz = make_us_eastern();
  // July 4, 2026 12:00 UTC - definitely in DST
  time_t summer = make_utc(2026, 7, 4, 12);
  EXPECT_TRUE(is_in_dst(summer, tz));
}

TEST(PosixTz, IsInDstUSEasternWinter) {
  auto tz = make_us_eastern();
  // January 15, 2026 12:00 UTC - definitely not in DST
  time_t winter = make_utc(2026, 1, 15, 12);
  EXPECT_FALSE(is_in_dst(winter, tz));
}

TEST(PosixTz, IsInDstNoDstTimezone) {
  // India: IST-5:30 (no DST)
  ParsedTimezone tz{};
  tz.std_offset_seconds = -(5 * 3600 + 30 * 60);
  // No DST rules

  time_t epoch = make_utc(2026, 7, 15, 12);
  EXPECT_FALSE(is_in_dst(epoch, tz));
}

TEST(PosixTz, SouthernHemisphereDstSummer) {
  auto tz = make_new_zealand();
  // December 15, 2025 12:00 UTC - summer in NZ, should be in DST
  time_t nz_summer = make_utc(2025, 12, 15, 12);
  EXPECT_TRUE(is_in_dst(nz_summer, tz));
}

TEST(PosixTz, SouthernHemisphereDstWinter) {
  auto tz = make_new_zealand();
  // July 15, 2026 12:00 UTC - winter in NZ, should NOT be in DST
  time_t nz_winter = make_utc(2026, 7, 15, 12);
  EXPECT_FALSE(is_in_dst(nz_winter, tz));
}

// ============================================================================
// epoch_to_local_tm tests
// ============================================================================

TEST(PosixTz, EpochToLocalBasic) {
  ParsedTimezone tz{};  // UTC

  time_t epoch = 0;  // Jan 1, 1970 00:00:00 UTC
  struct tm local;
  ASSERT_TRUE(epoch_to_local_tm(epoch, tz, &local));
  EXPECT_EQ(local.tm_year, 70);
  EXPECT_EQ(local.tm_mon, 0);
  EXPECT_EQ(local.tm_mday, 1);
  EXPECT_EQ(local.tm_hour, 0);
}

TEST(PosixTz, EpochToLocalNegativeEpoch) {
  ParsedTimezone tz{};  // UTC

  // Dec 31, 1969 23:59:59 UTC (1 second before epoch)
  time_t epoch = -1;
  struct tm local;
  ASSERT_TRUE(epoch_to_local_tm(epoch, tz, &local));
  EXPECT_EQ(local.tm_year, 69);  // 1969
  EXPECT_EQ(local.tm_mon, 11);   // December
  EXPECT_EQ(local.tm_mday, 31);
  EXPECT_EQ(local.tm_hour, 23);
  EXPECT_EQ(local.tm_min, 59);
  EXPECT_EQ(local.tm_sec, 59);
}

TEST(PosixTz, EpochToLocalNullTmFails) {
  ParsedTimezone tz{};
  EXPECT_FALSE(epoch_to_local_tm(0, tz, nullptr));
}

TEST(PosixTz, EpochToLocalWithOffset) {
  // EST5 (UTC-5, no DST)
  ParsedTimezone tz{};
  tz.std_offset_seconds = 5 * 3600;

  // Jan 1, 2026 05:00:00 UTC should be Jan 1, 2026 00:00:00 EST
  time_t utc_epoch = make_utc(2026, 1, 1, 5);

  struct tm local;
  ASSERT_TRUE(epoch_to_local_tm(utc_epoch, tz, &local));
  EXPECT_EQ(local.tm_hour, 0);  // Midnight EST
  EXPECT_EQ(local.tm_mday, 1);
  EXPECT_EQ(local.tm_isdst, 0);
}

TEST(PosixTz, EpochToLocalDstTransition) {
  auto tz = make_us_eastern();

  // July 4, 2026 16:00 UTC = 12:00 EDT (noon)
  time_t utc_epoch = make_utc(2026, 7, 4, 16);

  struct tm local;
  ASSERT_TRUE(epoch_to_local_tm(utc_epoch, tz, &local));
  EXPECT_EQ(local.tm_hour, 12);  // Noon EDT
  EXPECT_EQ(local.tm_isdst, 1);
}

// ============================================================================
// DST boundary edge cases
// ============================================================================

TEST(PosixTz, DstBoundaryJustBeforeSpringForward) {
  auto tz = make_us_eastern();

  // March 8, 2026 06:59:59 UTC = 01:59:59 EST (1 second before spring forward)
  time_t before_epoch = make_utc(2026, 3, 8, 6, 59, 59);
  EXPECT_FALSE(is_in_dst(before_epoch, tz));

  // March 8, 2026 07:00:00 UTC = 02:00:00 EST -> 03:00:00 EDT (DST started)
  time_t after_epoch = make_utc(2026, 3, 8, 7);
  EXPECT_TRUE(is_in_dst(after_epoch, tz));
}

TEST(PosixTz, DstBoundaryJustBeforeFallBack) {
  auto tz = make_us_eastern();

  // November 1, 2026 05:59:59 UTC = 01:59:59 EDT (1 second before fall back)
  time_t before_epoch = make_utc(2026, 11, 1, 5, 59, 59);
  EXPECT_TRUE(is_in_dst(before_epoch, tz));

  // November 1, 2026 06:00:00 UTC = 02:00:00 EDT -> 01:00:00 EST (DST ended)
  time_t after_epoch = make_utc(2026, 11, 1, 6);
  EXPECT_FALSE(is_in_dst(after_epoch, tz));
}

}  // namespace esphome::time::testing

// ============================================================================
// ESPTime::strptime tests (replaces sscanf-based parsing)
// ============================================================================

namespace esphome::testing {

TEST(ESPTimeStrptime, FullDateTime) {
  ESPTime t{};
  ASSERT_TRUE(ESPTime::strptime("2026-03-15 14:30:45", 19, t));
  EXPECT_EQ(t.year, 2026);
  EXPECT_EQ(t.month, 3);
  EXPECT_EQ(t.day_of_month, 15);
  EXPECT_EQ(t.hour, 14);
  EXPECT_EQ(t.minute, 30);
  EXPECT_EQ(t.second, 45);
}

TEST(ESPTimeStrptime, DateTimeNoSeconds) {
  ESPTime t{};
  ASSERT_TRUE(ESPTime::strptime("2026-03-15 14:30", 16, t));
  EXPECT_EQ(t.year, 2026);
  EXPECT_EQ(t.month, 3);
  EXPECT_EQ(t.day_of_month, 15);
  EXPECT_EQ(t.hour, 14);
  EXPECT_EQ(t.minute, 30);
  EXPECT_EQ(t.second, 0);
}

TEST(ESPTimeStrptime, DateOnly) {
  ESPTime t{};
  ASSERT_TRUE(ESPTime::strptime("2026-03-15", 10, t));
  EXPECT_EQ(t.year, 2026);
  EXPECT_EQ(t.month, 3);
  EXPECT_EQ(t.day_of_month, 15);
}

TEST(ESPTimeStrptime, TimeWithSeconds) {
  ESPTime t{};
  ASSERT_TRUE(ESPTime::strptime("14:30:45", 8, t));
  EXPECT_EQ(t.hour, 14);
  EXPECT_EQ(t.minute, 30);
  EXPECT_EQ(t.second, 45);
}

TEST(ESPTimeStrptime, TimeNoSeconds) {
  ESPTime t{};
  ASSERT_TRUE(ESPTime::strptime("14:30", 5, t));
  EXPECT_EQ(t.hour, 14);
  EXPECT_EQ(t.minute, 30);
  EXPECT_EQ(t.second, 0);
}

TEST(ESPTimeStrptime, Midnight) {
  ESPTime t{};
  ASSERT_TRUE(ESPTime::strptime("00:00:00", 8, t));
  EXPECT_EQ(t.hour, 0);
  EXPECT_EQ(t.minute, 0);
  EXPECT_EQ(t.second, 0);
}

TEST(ESPTimeStrptime, EndOfDay) {
  ESPTime t{};
  ASSERT_TRUE(ESPTime::strptime("23:59:59", 8, t));
  EXPECT_EQ(t.hour, 23);
  EXPECT_EQ(t.minute, 59);
  EXPECT_EQ(t.second, 59);
}

TEST(ESPTimeStrptime, LeapYearDate) {
  ESPTime t{};
  ASSERT_TRUE(ESPTime::strptime("2024-02-29", 10, t));
  EXPECT_EQ(t.year, 2024);
  EXPECT_EQ(t.month, 2);
  EXPECT_EQ(t.day_of_month, 29);
}

TEST(ESPTimeStrptime, NewYearsEve) {
  ESPTime t{};
  ASSERT_TRUE(ESPTime::strptime("2026-12-31 23:59:59", 19, t));
  EXPECT_EQ(t.year, 2026);
  EXPECT_EQ(t.month, 12);
  EXPECT_EQ(t.day_of_month, 31);
  EXPECT_EQ(t.hour, 23);
  EXPECT_EQ(t.minute, 59);
  EXPECT_EQ(t.second, 59);
}

TEST(ESPTimeStrptime, EmptyStringFails) {
  ESPTime t{};
  EXPECT_FALSE(ESPTime::strptime("", 0, t));
}

TEST(ESPTimeStrptime, NullInputFails) {
  ESPTime t{};
  EXPECT_FALSE(ESPTime::strptime(nullptr, 0, t));
}

TEST(ESPTimeStrptime, InvalidFormatFails) {
  ESPTime t{};
  EXPECT_FALSE(ESPTime::strptime("not-a-date", 10, t));
}

TEST(ESPTimeStrptime, PartialDateFails) {
  ESPTime t{};
  EXPECT_FALSE(ESPTime::strptime("2026-03", 7, t));
}

TEST(ESPTimeStrptime, PartialTimeFails) {
  ESPTime t{};
  EXPECT_FALSE(ESPTime::strptime("14:", 3, t));
}

TEST(ESPTimeStrptime, ExtraCharactersFails) {
  ESPTime t{};
  EXPECT_FALSE(ESPTime::strptime("2026-03-15 14:30:45x", 20, t));
}

TEST(ESPTimeStrptime, WrongSeparatorFails) {
  ESPTime t{};
  EXPECT_FALSE(ESPTime::strptime("2026/03/15", 10, t));
}

TEST(ESPTimeStrptime, LeadingZeroTime) {
  ESPTime t{};
  ASSERT_TRUE(ESPTime::strptime("01:05:09", 8, t));
  EXPECT_EQ(t.hour, 1);
  EXPECT_EQ(t.minute, 5);
  EXPECT_EQ(t.second, 9);
}

// ============================================================================
// recalc_timestamp_local() tests - verify behavior matches libc mktime()
// ============================================================================

}  // namespace esphome::testing

namespace esphome::time::testing {

using esphome::ESPTime;

// Helper to call libc mktime with same fields
static time_t libc_mktime(int year, int month, int day, int hour, int min, int sec) {
  struct tm tm {};
  tm.tm_year = year - 1900;
  tm.tm_mon = month - 1;
  tm.tm_mday = day;
  tm.tm_hour = hour;
  tm.tm_min = min;
  tm.tm_sec = sec;
  tm.tm_isdst = -1;  // Let libc determine DST
  return mktime(&tm);
}

// Helper to create ESPTime and call recalc_timestamp_local
static time_t esptime_recalc_local(int year, int month, int day, int hour, int min, int sec) {
  ESPTime t{};
  t.year = year;
  t.month = month;
  t.day_of_month = day;
  t.hour = hour;
  t.minute = min;
  t.second = sec;
  t.recalc_timestamp_local();
  return t.timestamp;
}

TEST(RecalcTimestampLocal, NormalTimeMatchesLibc) {
  // Set timezone to US Central (CST6CDT)
  const char *tz_str = "CST6CDT,M3.2.0,M11.1.0";
  setenv("TZ", tz_str, 1);
  tzset();
  auto tz = make_us_central();
  set_global_tz(tz);

  // Test a normal time in winter (no DST)
  // January 15, 2026 at 10:30:00 CST
  time_t libc_result = libc_mktime(2026, 1, 15, 10, 30, 0);
  time_t esp_result = esptime_recalc_local(2026, 1, 15, 10, 30, 0);
  EXPECT_EQ(esp_result, libc_result);

  // Test a normal time in summer (DST active)
  // July 15, 2026 at 10:30:00 CDT
  libc_result = libc_mktime(2026, 7, 15, 10, 30, 0);
  esp_result = esptime_recalc_local(2026, 7, 15, 10, 30, 0);
  EXPECT_EQ(esp_result, libc_result);
}

TEST(RecalcTimestampLocal, SpringForwardSkippedHour) {
  // Set timezone to US Central (CST6CDT)
  // DST starts March 8, 2026 at 2:00 AM -> clocks jump to 3:00 AM
  const char *tz_str = "CST6CDT,M3.2.0,M11.1.0";
  setenv("TZ", tz_str, 1);
  tzset();
  auto tz = make_us_central();
  set_global_tz(tz);

  // Test time before the transition (1:30 AM CST exists)
  time_t libc_result = libc_mktime(2026, 3, 8, 1, 30, 0);
  time_t esp_result = esptime_recalc_local(2026, 3, 8, 1, 30, 0);
  EXPECT_EQ(esp_result, libc_result);

  // Test time after the transition (3:30 AM CDT exists)
  libc_result = libc_mktime(2026, 3, 8, 3, 30, 0);
  esp_result = esptime_recalc_local(2026, 3, 8, 3, 30, 0);
  EXPECT_EQ(esp_result, libc_result);

  // Test the skipped hour (2:30 AM doesn't exist - gets normalized)
  // Both implementations should produce the same result
  libc_result = libc_mktime(2026, 3, 8, 2, 30, 0);
  esp_result = esptime_recalc_local(2026, 3, 8, 2, 30, 0);
  EXPECT_EQ(esp_result, libc_result);
}

TEST(RecalcTimestampLocal, FallBackRepeatedHour) {
  // Set timezone to US Central (CST6CDT)
  // DST ends November 1, 2026 at 2:00 AM -> clocks fall back to 1:00 AM
  const char *tz_str = "CST6CDT,M3.2.0,M11.1.0";
  setenv("TZ", tz_str, 1);
  tzset();
  auto tz = make_us_central();
  set_global_tz(tz);

  // Test time before the transition (midnight CDT)
  time_t libc_result = libc_mktime(2026, 11, 1, 0, 30, 0);
  time_t esp_result = esptime_recalc_local(2026, 11, 1, 0, 30, 0);
  EXPECT_EQ(esp_result, libc_result);

  // Test time well after the transition (3:00 AM CST)
  libc_result = libc_mktime(2026, 11, 1, 3, 0, 0);
  esp_result = esptime_recalc_local(2026, 11, 1, 3, 0, 0);
  EXPECT_EQ(esp_result, libc_result);

  // Test the repeated hour (1:30 AM occurs twice)
  // libc behavior varies by platform for this edge case, so we verify our
  // consistent behavior: prefer standard time (later UTC timestamp)
  esp_result = esptime_recalc_local(2026, 11, 1, 1, 30, 0);
  time_t std_interpretation = esptime_recalc_local(2026, 11, 1, 2, 30, 0) - 3600;  // 2:30 CST - 1 hour
  EXPECT_EQ(esp_result, std_interpretation);
}

TEST(RecalcTimestampLocal, SouthernHemisphereDST) {
  // Set timezone to Australia/Sydney (AEST-10AEDT,M10.1.0,M4.1.0/3)
  // DST starts first Sunday of October, ends first Sunday of April
  const char *tz_str = "AEST-10AEDT,M10.1.0,M4.1.0/3";
  setenv("TZ", tz_str, 1);
  tzset();
  auto tz = make_australia_sydney();
  set_global_tz(tz);

  // Test winter time (July - no DST in southern hemisphere)
  time_t libc_result = libc_mktime(2026, 7, 15, 10, 30, 0);
  time_t esp_result = esptime_recalc_local(2026, 7, 15, 10, 30, 0);
  EXPECT_EQ(esp_result, libc_result);

  // Test summer time (January - DST active in southern hemisphere)
  libc_result = libc_mktime(2026, 1, 15, 10, 30, 0);
  esp_result = esptime_recalc_local(2026, 1, 15, 10, 30, 0);
  EXPECT_EQ(esp_result, libc_result);
}

TEST(RecalcTimestampLocal, ExactTransitionBoundary) {
  // Test exact boundary of spring forward transition
  // Mar 8, 2026 at 2:00 AM CST -> 3:00 AM CDT (clocks skip forward)
  const char *tz_str = "CST6CDT,M3.2.0,M11.1.0";
  setenv("TZ", tz_str, 1);
  tzset();
  auto tz = make_us_central();
  set_global_tz(tz);

  // 1:59:59 AM CST - last second before transition (still standard time)
  time_t libc_result = libc_mktime(2026, 3, 8, 1, 59, 59);
  time_t esp_result = esptime_recalc_local(2026, 3, 8, 1, 59, 59);
  EXPECT_EQ(esp_result, libc_result);

  // 3:00:00 AM CDT - first second after transition (now DST)
  libc_result = libc_mktime(2026, 3, 8, 3, 0, 0);
  esp_result = esptime_recalc_local(2026, 3, 8, 3, 0, 0);
  EXPECT_EQ(esp_result, libc_result);

  // Verify the gap: 3:00 AM CDT should be exactly 1 second after 1:59:59 AM CST
  time_t before_transition = esptime_recalc_local(2026, 3, 8, 1, 59, 59);
  time_t after_transition = esptime_recalc_local(2026, 3, 8, 3, 0, 0);
  EXPECT_EQ(after_transition - before_transition, 1);
}

TEST(RecalcTimestampLocal, NonDefaultTransitionTime) {
  // Test DST transition at 3:00 AM instead of default 2:00 AM
  // Using custom transition time: CST6CDT,M3.2.0/3,M11.1.0/3
  const char *tz_str = "CST6CDT,M3.2.0/3,M11.1.0/3";
  setenv("TZ", tz_str, 1);
  tzset();
  ParsedTimezone tz{};
  tz.std_offset_seconds = 6 * 3600;
  tz.dst_offset_seconds = 5 * 3600;
  tz.dst_start.type = DSTRuleType::MONTH_WEEK_DAY;
  tz.dst_start.month = 3;
  tz.dst_start.week = 2;
  tz.dst_start.day_of_week = 0;
  tz.dst_start.time_seconds = 3 * 3600;
  tz.dst_end.type = DSTRuleType::MONTH_WEEK_DAY;
  tz.dst_end.month = 11;
  tz.dst_end.week = 1;
  tz.dst_end.day_of_week = 0;
  tz.dst_end.time_seconds = 3 * 3600;
  set_global_tz(tz);

  // 2:30 AM should still be standard time (transition at 3:00 AM)
  time_t libc_result = libc_mktime(2026, 3, 8, 2, 30, 0);
  time_t esp_result = esptime_recalc_local(2026, 3, 8, 2, 30, 0);
  EXPECT_EQ(esp_result, libc_result);

  // 4:00 AM should be DST (after 3:00 AM transition)
  libc_result = libc_mktime(2026, 3, 8, 4, 0, 0);
  esp_result = esptime_recalc_local(2026, 3, 8, 4, 0, 0);
  EXPECT_EQ(esp_result, libc_result);
}

TEST(RecalcTimestampLocal, MinimalFieldsWithoutDayOfWeekOrYear) {
  // Regression test for issue #15115: DateTimeEntity::state_as_esptime() constructs
  // an ESPTime with only year/month/day/hour/minute/second set (no day_of_week or
  // day_of_year). recalc_timestamp_local() must work without those fields.
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0", 1);
  tzset();
  time::ParsedTimezone tz{};
  tz.std_offset_seconds = -1 * 3600;  // CET-1 = UTC+1
  tz.dst_offset_seconds = -2 * 3600;  // CEST = UTC+2
  tz.dst_start.type = DSTRuleType::MONTH_WEEK_DAY;
  tz.dst_start.month = 3;
  tz.dst_start.week = 5;
  tz.dst_start.day_of_week = 0;
  tz.dst_start.time_seconds = 2 * 3600;
  tz.dst_end.type = DSTRuleType::MONTH_WEEK_DAY;
  tz.dst_end.month = 10;
  tz.dst_end.week = 5;
  tz.dst_end.day_of_week = 0;
  tz.dst_end.time_seconds = 2 * 3600;
  set_global_tz(tz);

  // Construct ESPTime with only date/time fields (like state_as_esptime does)
  ESPTime t{};
  t.year = 2026;
  t.month = 3;
  t.day_of_month = 20;
  t.hour = 23;
  t.minute = 14;
  t.second = 55;
  // day_of_week and day_of_year are deliberately left as 0
  t.recalc_timestamp_local();

  // Must NOT return -1 (the bug: fields_in_range() rejected valid times)
  EXPECT_NE(t.timestamp, -1);

  // Verify against libc
  time_t libc_result = libc_mktime(2026, 3, 20, 23, 14, 55);
  EXPECT_EQ(t.timestamp, libc_result);
}

TEST(RecalcTimestampLocal, MinimalFieldsNoDST) {
  // Same test but with a timezone that has no DST
  setenv("TZ", "IST-5:30", 1);
  tzset();
  time::ParsedTimezone tz{};
  tz.std_offset_seconds = -(5 * 3600 + 30 * 60);  // IST-5:30 = UTC+5:30
  // No DST
  set_global_tz(tz);

  ESPTime t{};
  t.year = 2026;
  t.month = 3;
  t.day_of_month = 23;
  t.hour = 10;
  t.minute = 0;
  t.second = 0;
  t.recalc_timestamp_local();

  EXPECT_NE(t.timestamp, -1);

  time_t libc_result = libc_mktime(2026, 3, 23, 10, 0, 0);
  EXPECT_EQ(t.timestamp, libc_result);
}

TEST(RecalcTimestampLocal, YearBoundaryDST) {
  // Test southern hemisphere DST across year boundary
  // Australia/Sydney: DST active from October to April (spans Jan 1)
  const char *tz_str = "AEST-10AEDT,M10.1.0,M4.1.0/3";
  setenv("TZ", tz_str, 1);
  tzset();
  auto tz = make_australia_sydney();
  set_global_tz(tz);

  // Dec 31, 2025 at 23:30 - DST should be active
  time_t libc_result = libc_mktime(2025, 12, 31, 23, 30, 0);
  time_t esp_result = esptime_recalc_local(2025, 12, 31, 23, 30, 0);
  EXPECT_EQ(esp_result, libc_result);

  // Jan 1, 2026 at 00:30 - DST should still be active
  libc_result = libc_mktime(2026, 1, 1, 0, 30, 0);
  esp_result = esptime_recalc_local(2026, 1, 1, 0, 30, 0);
  EXPECT_EQ(esp_result, libc_result);

  // Verify both are in DST (11 hour offset from UTC, not 10)
  // The timestamps should be 1 hour apart
  time_t dec31 = esptime_recalc_local(2025, 12, 31, 23, 30, 0);
  time_t jan1 = esptime_recalc_local(2026, 1, 1, 0, 30, 0);
  EXPECT_EQ(jan1 - dec31, 3600);  // 1 hour difference
}

// ============================================================================
// ESPTime::timezone_offset() tests
// ============================================================================

TEST(TimezoneOffset, NoTimezone) {
  ParsedTimezone tz{};
  set_global_tz(tz);

  int32_t offset = ESPTime::timezone_offset();
  EXPECT_EQ(offset, 0);
}

TEST(TimezoneOffset, FixedOffsetPositive) {
  // India: IST-5:30 (no DST)
  ParsedTimezone tz{};
  tz.std_offset_seconds = -(5 * 3600 + 30 * 60);
  set_global_tz(tz);

  int32_t offset = ESPTime::timezone_offset();
  EXPECT_EQ(offset, 5 * 3600 + 30 * 60);
}

TEST(TimezoneOffset, FixedOffsetNegative) {
  // EST5 (no DST)
  ParsedTimezone tz{};
  tz.std_offset_seconds = 5 * 3600;
  set_global_tz(tz);

  int32_t offset = ESPTime::timezone_offset();
  EXPECT_EQ(offset, -5 * 3600);
}

TEST(TimezoneOffset, WithDstReturnsCorrectOffsetBasedOnCurrentTime) {
  // US Eastern with DST
  auto tz = make_us_eastern();
  set_global_tz(tz);

  // Get current time and check offset matches expected based on DST status
  time_t now = ::time(nullptr);
  int32_t offset = ESPTime::timezone_offset();

  // Verify offset matches what is_in_dst says
  if (is_in_dst(now, tz)) {
    // During DST, offset should be -4 hours (EDT)
    EXPECT_EQ(offset, -4 * 3600);
  } else {
    // During standard time, offset should be -5 hours (EST)
    EXPECT_EQ(offset, -5 * 3600);
  }
}

// ============================================================================
// Leap year edge cases for closed-form year arithmetic
// ============================================================================

TEST(PosixTz, EpochToLocalLeapYear2000) {
  // 2000 is a leap year (divisible by 400)
  ParsedTimezone tz{};  // UTC

  // Feb 29, 2000 12:00:00 UTC
  time_t epoch = make_utc(2000, 2, 29, 12);
  struct tm local;
  ASSERT_TRUE(epoch_to_local_tm(epoch, tz, &local));
  EXPECT_EQ(local.tm_year, 100);  // 2000
  EXPECT_EQ(local.tm_mon, 1);     // February
  EXPECT_EQ(local.tm_mday, 29);
  EXPECT_EQ(local.tm_hour, 12);
}

TEST(PosixTz, EpochToLocalNonLeapYear2100) {
  // 2100 is NOT a leap year (divisible by 100 but not 400)
  ParsedTimezone tz{};  // UTC

  // Mar 1, 2100 00:00:00 UTC — the day after what would be Feb 29
  time_t epoch = make_utc(2100, 3, 1);
  struct tm local;
  ASSERT_TRUE(epoch_to_local_tm(epoch, tz, &local));
  EXPECT_EQ(local.tm_year, 200);  // 2100
  EXPECT_EQ(local.tm_mon, 2);     // March
  EXPECT_EQ(local.tm_mday, 1);

  // Feb 28, 2100 23:59:59 UTC — last second of February (no Feb 29)
  epoch = make_utc(2100, 2, 28, 23, 59, 59);
  ASSERT_TRUE(epoch_to_local_tm(epoch, tz, &local));
  EXPECT_EQ(local.tm_year, 200);
  EXPECT_EQ(local.tm_mon, 1);  // February
  EXPECT_EQ(local.tm_mday, 28);
}

TEST(PosixTz, EpochToLocalLeapYear2400) {
  // 2400 is a leap year (divisible by 400)
  ParsedTimezone tz{};  // UTC

  time_t epoch = make_utc(2400, 2, 29, 6);
  struct tm local;
  ASSERT_TRUE(epoch_to_local_tm(epoch, tz, &local));
  EXPECT_EQ(local.tm_year, 500);  // 2400
  EXPECT_EQ(local.tm_mon, 1);     // February
  EXPECT_EQ(local.tm_mday, 29);
  EXPECT_EQ(local.tm_hour, 6);
}

TEST(PosixTz, EpochToLocalNewYearBoundaries) {
  // Test year boundary — last second of 2099 and first second of 2100
  ParsedTimezone tz{};  // UTC
  struct tm local;

  // Dec 31, 2099 23:59:59 UTC
  time_t epoch = make_utc(2099, 12, 31, 23, 59, 59);
  ASSERT_TRUE(epoch_to_local_tm(epoch, tz, &local));
  EXPECT_EQ(local.tm_year, 199);  // 2099
  EXPECT_EQ(local.tm_mon, 11);    // December
  EXPECT_EQ(local.tm_mday, 31);

  // Jan 1, 2100 00:00:00 UTC
  epoch = make_utc(2100, 1, 1);
  ASSERT_TRUE(epoch_to_local_tm(epoch, tz, &local));
  EXPECT_EQ(local.tm_year, 200);  // 2100
  EXPECT_EQ(local.tm_mon, 0);     // January
  EXPECT_EQ(local.tm_mday, 1);
}

TEST(PosixTz, EpochToLocalDstAcrossCenturyBoundary) {
  // DST transition in year 2100 (non-leap) with US Eastern rules
  ParsedTimezone tz = make_us_eastern();

  // July 4, 2100 16:00 UTC = 12:00 EDT
  time_t epoch = make_utc(2100, 7, 4, 16);
  struct tm local;
  ASSERT_TRUE(epoch_to_local_tm(epoch, tz, &local));
  EXPECT_EQ(local.tm_hour, 12);
  EXPECT_EQ(local.tm_isdst, 1);

  // Jan 15, 2100 10:00 UTC = 05:00 EST
  epoch = make_utc(2100, 1, 15, 10);
  ASSERT_TRUE(epoch_to_local_tm(epoch, tz, &local));
  EXPECT_EQ(local.tm_hour, 5);
  EXPECT_EQ(local.tm_isdst, 0);
}

TEST(PosixTz, EpochToLocalFarFutureYear5000) {
  // Year 5000 — days/365 estimate overshoots by ~2 years due to leap days,
  // requiring multiple correction steps in days_to_year.
  ParsedTimezone tz{};  // UTC

  time_t epoch = make_utc(5000, 6, 15, 12);
  struct tm local;
  ASSERT_TRUE(epoch_to_local_tm(epoch, tz, &local));
  EXPECT_EQ(local.tm_year, 3100);  // 5000
  EXPECT_EQ(local.tm_mon, 5);      // June
  EXPECT_EQ(local.tm_mday, 15);
  EXPECT_EQ(local.tm_hour, 12);
}

// ============================================================================
// Verification against libc
// ============================================================================

// Helper to build the <+07>-7 timezone (UTC+7, no DST)
static ParsedTimezone make_plus_seven() {
  ParsedTimezone tz{};
  tz.std_offset_seconds = -7 * 3600;
  return tz;
}

// Helper to build the India timezone (IST-5:30, no DST)
static ParsedTimezone make_india() {
  ParsedTimezone tz{};
  tz.std_offset_seconds = -(5 * 3600 + 30 * 60);
  return tz;
}

// Helper to build the Europe/Berlin timezone (CET-1CEST,M3.5.0,M10.5.0/3)
static ParsedTimezone make_europe_berlin() {
  ParsedTimezone tz{};
  tz.std_offset_seconds = -1 * 3600;
  tz.dst_offset_seconds = -2 * 3600;
  tz.dst_start.type = DSTRuleType::MONTH_WEEK_DAY;
  tz.dst_start.month = 3;
  tz.dst_start.week = 5;
  tz.dst_start.day_of_week = 0;
  tz.dst_start.time_seconds = 2 * 3600;
  tz.dst_end.type = DSTRuleType::MONTH_WEEK_DAY;
  tz.dst_end.month = 10;
  tz.dst_end.week = 5;
  tz.dst_end.day_of_week = 0;
  tz.dst_end.time_seconds = 3 * 3600;
  return tz;
}

// Compares our converter against libc for the same zone: the struct drives
// epoch_to_local_tm() and the equivalent POSIX TZ string drives libc localtime().
using LibcVerificationParam = std::tuple<ParsedTimezone (*)(), const char *, time_t>;

class LibcVerificationTest : public ::testing::TestWithParam<LibcVerificationParam> {
 protected:
  // NOLINTNEXTLINE(readability-identifier-naming) - Google Test requires this name
  void SetUp() override {
    // Save current TZ
    const char *current_tz = getenv("TZ");
    saved_tz_ = current_tz ? current_tz : "";
    had_tz_ = current_tz != nullptr;
  }

  // NOLINTNEXTLINE(readability-identifier-naming) - Google Test requires this name
  void TearDown() override {
    // Restore TZ
    if (had_tz_) {
      setenv("TZ", saved_tz_.c_str(), 1);
    } else {
      unsetenv("TZ");
    }
    tzset();
  }

 private:
  std::string saved_tz_;
  bool had_tz_{false};
};

TEST_P(LibcVerificationTest, MatchesLibc) {
  auto [make_tz, tz_str, epoch] = GetParam();

  ParsedTimezone tz = make_tz();

  // Our implementation
  struct tm our_tm {};
  ASSERT_TRUE(epoch_to_local_tm(epoch, tz, &our_tm));

  // libc implementation
  setenv("TZ", tz_str, 1);
  tzset();
  struct tm *libc_tm = localtime(&epoch);
  ASSERT_NE(libc_tm, nullptr);

  EXPECT_EQ(our_tm.tm_year, libc_tm->tm_year);
  EXPECT_EQ(our_tm.tm_mon, libc_tm->tm_mon);
  EXPECT_EQ(our_tm.tm_mday, libc_tm->tm_mday);
  EXPECT_EQ(our_tm.tm_hour, libc_tm->tm_hour);
  EXPECT_EQ(our_tm.tm_min, libc_tm->tm_min);
  EXPECT_EQ(our_tm.tm_sec, libc_tm->tm_sec);
  EXPECT_EQ(our_tm.tm_isdst, libc_tm->tm_isdst);
}

INSTANTIATE_TEST_SUITE_P(USEastern, LibcVerificationTest,
                         ::testing::Values(std::make_tuple(&make_us_eastern, "EST5EDT,M3.2.0/2,M11.1.0/2", 1704067200),
                                           std::make_tuple(&make_us_eastern, "EST5EDT,M3.2.0/2,M11.1.0/2", 1720000000),
                                           std::make_tuple(&make_us_eastern, "EST5EDT,M3.2.0/2,M11.1.0/2",
                                                           1735689600)));

INSTANTIATE_TEST_SUITE_P(AngleBracket, LibcVerificationTest,
                         ::testing::Values(std::make_tuple(&make_plus_seven, "<+07>-7", 1704067200),
                                           std::make_tuple(&make_plus_seven, "<+07>-7", 1720000000)));

INSTANTIATE_TEST_SUITE_P(India, LibcVerificationTest,
                         ::testing::Values(std::make_tuple(&make_india, "IST-5:30", 1704067200),
                                           std::make_tuple(&make_india, "IST-5:30", 1720000000)));

INSTANTIATE_TEST_SUITE_P(
    NewZealand, LibcVerificationTest,
    ::testing::Values(std::make_tuple(&make_new_zealand, "NZST-12NZDT,M9.5.0,M4.1.0/3", 1704067200),
                      std::make_tuple(&make_new_zealand, "NZST-12NZDT,M9.5.0,M4.1.0/3", 1720000000)));

INSTANTIATE_TEST_SUITE_P(USCentral, LibcVerificationTest,
                         ::testing::Values(std::make_tuple(&make_us_central, "CST6CDT,M3.2.0/2,M11.1.0/2", 1704067200),
                                           std::make_tuple(&make_us_central, "CST6CDT,M3.2.0/2,M11.1.0/2", 1720000000),
                                           std::make_tuple(&make_us_central, "CST6CDT,M3.2.0/2,M11.1.0/2",
                                                           1735689600)));

INSTANTIATE_TEST_SUITE_P(
    EuropeBerlin, LibcVerificationTest,
    ::testing::Values(std::make_tuple(&make_europe_berlin, "CET-1CEST,M3.5.0,M10.5.0/3", 1704067200),
                      std::make_tuple(&make_europe_berlin, "CET-1CEST,M3.5.0,M10.5.0/3", 1720000000),
                      std::make_tuple(&make_europe_berlin, "CET-1CEST,M3.5.0,M10.5.0/3", 1735689600)));

INSTANTIATE_TEST_SUITE_P(
    AustraliaSydney, LibcVerificationTest,
    ::testing::Values(std::make_tuple(&make_australia_sydney, "AEST-10AEDT,M10.1.0,M4.1.0/3", 1704067200),
                      std::make_tuple(&make_australia_sydney, "AEST-10AEDT,M10.1.0,M4.1.0/3", 1720000000),
                      std::make_tuple(&make_australia_sydney, "AEST-10AEDT,M10.1.0,M4.1.0/3", 1735689600)));

}  // namespace esphome::time::testing
