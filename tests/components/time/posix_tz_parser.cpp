// Tests for the POSIX TZ parser implementation
// This verifies our custom parser produces identical results to libc's
// tzset()/localtime() implementation. The custom parser avoids pulling in scanf (~7.6KB).

#include <gtest/gtest.h>
#include <cstdlib>
#include <ctime>
#include "esphome/components/time/posix_tz.h"

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

// ============================================================================
// Basic TZ string parsing tests
// ============================================================================

TEST(PosixTzParser, ParseSimpleOffsetEST5) {
  ParsedTimezone tz;
  ASSERT_TRUE(parse_posix_tz("EST5", tz));
  EXPECT_EQ(tz.std_offset_seconds, 5 * 3600);  // +5 hours (west of UTC)
  EXPECT_FALSE(tz.has_dst);
}

TEST(PosixTzParser, ParseNegativeOffsetCET) {
  ParsedTimezone tz;
  ASSERT_TRUE(parse_posix_tz("CET-1", tz));
  EXPECT_EQ(tz.std_offset_seconds, -1 * 3600);  // -1 hour (east of UTC)
  EXPECT_FALSE(tz.has_dst);
}

TEST(PosixTzParser, ParseExplicitPositiveOffset) {
  ParsedTimezone tz;
  ASSERT_TRUE(parse_posix_tz("TEST+5", tz));
  EXPECT_EQ(tz.std_offset_seconds, 5 * 3600);
  EXPECT_FALSE(tz.has_dst);
}

TEST(PosixTzParser, ParseZeroOffset) {
  ParsedTimezone tz;
  ASSERT_TRUE(parse_posix_tz("UTC0", tz));
  EXPECT_EQ(tz.std_offset_seconds, 0);
  EXPECT_FALSE(tz.has_dst);
}

TEST(PosixTzParser, ParseUSEasternWithDST) {
  ParsedTimezone tz;
  ASSERT_TRUE(parse_posix_tz("EST5EDT,M3.2.0,M11.1.0", tz));
  EXPECT_EQ(tz.std_offset_seconds, 5 * 3600);
  EXPECT_EQ(tz.dst_offset_seconds, 4 * 3600);  // Default: STD - 1hr
  EXPECT_TRUE(tz.has_dst);
  EXPECT_EQ(tz.dst_start.month, 3);
  EXPECT_EQ(tz.dst_start.week, 2);
  EXPECT_EQ(tz.dst_start.day_of_week, 0);          // Sunday
  EXPECT_EQ(tz.dst_start.time_seconds, 2 * 3600);  // Default 2:00 AM
  EXPECT_EQ(tz.dst_end.month, 11);
  EXPECT_EQ(tz.dst_end.week, 1);
  EXPECT_EQ(tz.dst_end.day_of_week, 0);
}

TEST(PosixTzParser, ParseUSCentralWithTime) {
  ParsedTimezone tz;
  ASSERT_TRUE(parse_posix_tz("CST6CDT,M3.2.0/2,M11.1.0/2", tz));
  EXPECT_EQ(tz.std_offset_seconds, 6 * 3600);
  EXPECT_EQ(tz.dst_offset_seconds, 5 * 3600);
  EXPECT_EQ(tz.dst_start.time_seconds, 2 * 3600);  // 2:00 AM
  EXPECT_EQ(tz.dst_end.time_seconds, 2 * 3600);
}

TEST(PosixTzParser, ParseEuropeBerlin) {
  ParsedTimezone tz;
  ASSERT_TRUE(parse_posix_tz("CET-1CEST,M3.5.0,M10.5.0/3", tz));
  EXPECT_EQ(tz.std_offset_seconds, -1 * 3600);
  EXPECT_EQ(tz.dst_offset_seconds, -2 * 3600);  // Default: STD - 1hr
  EXPECT_TRUE(tz.has_dst);
  EXPECT_EQ(tz.dst_start.month, 3);
  EXPECT_EQ(tz.dst_start.week, 5);  // Last week
  EXPECT_EQ(tz.dst_end.month, 10);
  EXPECT_EQ(tz.dst_end.week, 5);                 // Last week
  EXPECT_EQ(tz.dst_end.time_seconds, 3 * 3600);  // 3:00 AM
}

TEST(PosixTzParser, ParseNewZealand) {
  ParsedTimezone tz;
  // Southern hemisphere - DST starts in Sept, ends in April
  ASSERT_TRUE(parse_posix_tz("NZST-12NZDT,M9.5.0,M4.1.0/3", tz));
  EXPECT_EQ(tz.std_offset_seconds, -12 * 3600);
  EXPECT_EQ(tz.dst_offset_seconds, -13 * 3600);  // Default: STD - 1hr
  EXPECT_TRUE(tz.has_dst);
  EXPECT_EQ(tz.dst_start.month, 9);  // September
  EXPECT_EQ(tz.dst_end.month, 4);    // April
}

TEST(PosixTzParser, ParseExplicitDstOffset) {
  ParsedTimezone tz;
  // Some places have non-standard DST offsets
  ASSERT_TRUE(parse_posix_tz("TEST5DST4,M3.2.0,M11.1.0", tz));
  EXPECT_EQ(tz.std_offset_seconds, 5 * 3600);
  EXPECT_EQ(tz.dst_offset_seconds, 4 * 3600);
  EXPECT_TRUE(tz.has_dst);
}

// ============================================================================
// Angle-bracket notation tests (espressif/newlib-esp32#8)
// ============================================================================

TEST(PosixTzParser, ParseAngleBracketPositive) {
  // Format: <+07>-7 means UTC+7 (name is "+07", offset is -7 hours east)
  ParsedTimezone tz;
  ASSERT_TRUE(parse_posix_tz("<+07>-7", tz));
  EXPECT_EQ(tz.std_offset_seconds, -7 * 3600);  // -7 = 7 hours east of UTC
  EXPECT_FALSE(tz.has_dst);
}

TEST(PosixTzParser, ParseAngleBracketNegative) {
  // <-03>3 means UTC-3 (name is "-03", offset is 3 hours west)
  ParsedTimezone tz;
  ASSERT_TRUE(parse_posix_tz("<-03>3", tz));
  EXPECT_EQ(tz.std_offset_seconds, 3 * 3600);
  EXPECT_FALSE(tz.has_dst);
}

TEST(PosixTzParser, ParseAngleBracketWithDST) {
  // <+10>-10<+11>,M10.1.0,M4.1.0/3 (Australia/Sydney style)
  ParsedTimezone tz;
  ASSERT_TRUE(parse_posix_tz("<+10>-10<+11>,M10.1.0,M4.1.0/3", tz));
  EXPECT_EQ(tz.std_offset_seconds, -10 * 3600);
  EXPECT_EQ(tz.dst_offset_seconds, -11 * 3600);
  EXPECT_TRUE(tz.has_dst);
  EXPECT_EQ(tz.dst_start.month, 10);
  EXPECT_EQ(tz.dst_end.month, 4);
}

TEST(PosixTzParser, ParseAngleBracketNamed) {
  // <AEST>-10 (Australian Eastern Standard Time)
  ParsedTimezone tz;
  ASSERT_TRUE(parse_posix_tz("<AEST>-10", tz));
  EXPECT_EQ(tz.std_offset_seconds, -10 * 3600);
  EXPECT_FALSE(tz.has_dst);
}

TEST(PosixTzParser, ParseAngleBracketWithMinutes) {
  // <+0545>-5:45 (Nepal)
  ParsedTimezone tz;
  ASSERT_TRUE(parse_posix_tz("<+0545>-5:45", tz));
  EXPECT_EQ(tz.std_offset_seconds, -(5 * 3600 + 45 * 60));
  EXPECT_FALSE(tz.has_dst);
}

// ============================================================================
// Half-hour and unusual offset tests
// ============================================================================

TEST(PosixTzParser, ParseOffsetWithMinutesIndia) {
  ParsedTimezone tz;
  // India: UTC+5:30
  ASSERT_TRUE(parse_posix_tz("IST-5:30", tz));
  EXPECT_EQ(tz.std_offset_seconds, -(5 * 3600 + 30 * 60));
  EXPECT_FALSE(tz.has_dst);
}

TEST(PosixTzParser, ParseOffsetWithMinutesNepal) {
  ParsedTimezone tz;
  // Nepal: UTC+5:45
  ASSERT_TRUE(parse_posix_tz("NPT-5:45", tz));
  EXPECT_EQ(tz.std_offset_seconds, -(5 * 3600 + 45 * 60));
  EXPECT_FALSE(tz.has_dst);
}

TEST(PosixTzParser, ParseOffsetWithSeconds) {
  ParsedTimezone tz;
  // Unusual but valid: offset with seconds
  ASSERT_TRUE(parse_posix_tz("TEST-1:30:30", tz));
  EXPECT_EQ(tz.std_offset_seconds, -(1 * 3600 + 30 * 60 + 30));
}

TEST(PosixTzParser, ParseChathamIslands) {
  // Chatham Islands: UTC+12:45 with DST
  ParsedTimezone tz;
  ASSERT_TRUE(parse_posix_tz("<+1245>-12:45<+1345>,M9.5.0/2:45,M4.1.0/3:45", tz));
  EXPECT_EQ(tz.std_offset_seconds, -(12 * 3600 + 45 * 60));
  EXPECT_EQ(tz.dst_offset_seconds, -(13 * 3600 + 45 * 60));
  EXPECT_TRUE(tz.has_dst);
}

// ============================================================================
// Invalid input tests
// ============================================================================

TEST(PosixTzParser, ParseEmptyStringFails) {
  ParsedTimezone tz;
  EXPECT_FALSE(parse_posix_tz("", tz));
}

TEST(PosixTzParser, ParseNullFails) {
  ParsedTimezone tz;
  EXPECT_FALSE(parse_posix_tz(nullptr, tz));
}

TEST(PosixTzParser, ParseShortNameFails) {
  ParsedTimezone tz;
  // TZ name must be at least 3 characters
  EXPECT_FALSE(parse_posix_tz("AB5", tz));
}

TEST(PosixTzParser, ParseMissingOffsetFails) {
  ParsedTimezone tz;
  EXPECT_FALSE(parse_posix_tz("EST", tz));
}

TEST(PosixTzParser, ParseUnterminatedBracketFails) {
  ParsedTimezone tz;
  EXPECT_FALSE(parse_posix_tz("<+07-7", tz));  // Missing closing >
}

// ============================================================================
// J-format and plain day number tests
// ============================================================================

TEST(PosixTzParser, ParseJFormatBasic) {
  ParsedTimezone tz;
  // J format: Julian day 1-365, not counting Feb 29
  ASSERT_TRUE(parse_posix_tz("EST5EDT,J60,J305", tz));
  EXPECT_TRUE(tz.has_dst);
  EXPECT_EQ(tz.dst_start.type, DSTRuleType::JULIAN_NO_LEAP);
  EXPECT_EQ(tz.dst_start.day, 60);  // March 1
  EXPECT_EQ(tz.dst_end.type, DSTRuleType::JULIAN_NO_LEAP);
  EXPECT_EQ(tz.dst_end.day, 305);  // November 1
}

TEST(PosixTzParser, ParseJFormatWithTime) {
  ParsedTimezone tz;
  ASSERT_TRUE(parse_posix_tz("EST5EDT,J60/2,J305/2", tz));
  EXPECT_EQ(tz.dst_start.day, 60);
  EXPECT_EQ(tz.dst_start.time_seconds, 2 * 3600);
  EXPECT_EQ(tz.dst_end.day, 305);
  EXPECT_EQ(tz.dst_end.time_seconds, 2 * 3600);
}

TEST(PosixTzParser, ParsePlainDayNumber) {
  ParsedTimezone tz;
  // Plain format: day 0-365, counting Feb 29 in leap years
  ASSERT_TRUE(parse_posix_tz("EST5EDT,59,304", tz));
  EXPECT_TRUE(tz.has_dst);
  EXPECT_EQ(tz.dst_start.type, DSTRuleType::DAY_OF_YEAR);
  EXPECT_EQ(tz.dst_start.day, 59);
  EXPECT_EQ(tz.dst_end.type, DSTRuleType::DAY_OF_YEAR);
  EXPECT_EQ(tz.dst_end.day, 304);
}

TEST(PosixTzParser, JFormatInvalidDayZero) {
  ParsedTimezone tz;
  // J format day must be 1-365, not 0
  EXPECT_FALSE(parse_posix_tz("EST5EDT,J0,J305", tz));
}

TEST(PosixTzParser, JFormatInvalidDay366) {
  ParsedTimezone tz;
  // J format day must be 1-365
  EXPECT_FALSE(parse_posix_tz("EST5EDT,J366,J305", tz));
}

TEST(PosixTzParser, ParsePlainDayNumberWithTime) {
  ParsedTimezone tz;
  ASSERT_TRUE(parse_posix_tz("EST5EDT,59/3,304/1:30", tz));
  EXPECT_EQ(tz.dst_start.day, 59);
  EXPECT_EQ(tz.dst_start.time_seconds, 3 * 3600);
  EXPECT_EQ(tz.dst_end.day, 304);
  EXPECT_EQ(tz.dst_end.time_seconds, 1 * 3600 + 30 * 60);
}

TEST(PosixTzParser, PlainDayInvalidDay366) {
  ParsedTimezone tz;
  // Plain format day must be 0-365
  EXPECT_FALSE(parse_posix_tz("EST5EDT,366,304", tz));
}

// ============================================================================
// Helper function tests
// ============================================================================

TEST(PosixTzParser, JulianDay60IsMarch1) {
  // J60 is always March 1 (J format ignores leap years by design)
  int month, day;
  internal::julian_to_month_day(60, month, day);
  EXPECT_EQ(month, 3);
  EXPECT_EQ(day, 1);
}

TEST(PosixTzParser, DayOfYear59DiffersByLeap) {
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

TEST(PosixTzParser, DayOfWeekKnownDates) {
  // January 1, 1970 was Thursday (4)
  EXPECT_EQ(internal::day_of_week(1970, 1, 1), 4);
  // January 1, 2000 was Saturday (6)
  EXPECT_EQ(internal::day_of_week(2000, 1, 1), 6);
  // March 8, 2026 is Sunday (0) - US DST start
  EXPECT_EQ(internal::day_of_week(2026, 3, 8), 0);
}

TEST(PosixTzParser, LeapYearDetection) {
  EXPECT_FALSE(internal::is_leap_year(1900));  // Divisible by 100 but not 400
  EXPECT_TRUE(internal::is_leap_year(2000));   // Divisible by 400
  EXPECT_TRUE(internal::is_leap_year(2024));   // Divisible by 4
  EXPECT_FALSE(internal::is_leap_year(2025));  // Not divisible by 4
}

TEST(PosixTzParser, JulianDay1IsJan1) {
  int month, day;
  internal::julian_to_month_day(1, month, day);
  EXPECT_EQ(month, 1);
  EXPECT_EQ(day, 1);
}

TEST(PosixTzParser, JulianDay365IsDec31) {
  int month, day;
  internal::julian_to_month_day(365, month, day);
  EXPECT_EQ(month, 12);
  EXPECT_EQ(day, 31);
}

TEST(PosixTzParser, DayOfYear0IsJan1) {
  int month, day;
  internal::day_of_year_to_month_day(0, 2025, month, day);
  EXPECT_EQ(month, 1);
  EXPECT_EQ(day, 1);
}

TEST(PosixTzParser, DaysInMonthRegular) {
  EXPECT_EQ(internal::days_in_month(2025, 1), 31);
  EXPECT_EQ(internal::days_in_month(2025, 2), 28);
  EXPECT_EQ(internal::days_in_month(2025, 4), 30);
  EXPECT_EQ(internal::days_in_month(2025, 12), 31);
}

TEST(PosixTzParser, DaysInMonthLeapYear) {
  EXPECT_EQ(internal::days_in_month(2024, 2), 29);
  EXPECT_EQ(internal::days_in_month(2025, 2), 28);
}

// ============================================================================
// DST transition calculation tests
// ============================================================================

TEST(PosixTzParser, DstStartUSEastern2026) {
  // March 8, 2026 is 2nd Sunday of March
  ParsedTimezone tz;
  parse_posix_tz("EST5EDT,M3.2.0/2,M11.1.0/2", tz);

  time_t dst_start = calculate_dst_transition(2026, tz.dst_start, tz.std_offset_seconds);
  struct tm tm;
  internal::epoch_to_tm_utc(dst_start, &tm);

  // At 2:00 AM EST (UTC-5), so 7:00 AM UTC
  EXPECT_EQ(tm.tm_year + 1900, 2026);
  EXPECT_EQ(tm.tm_mon + 1, 3);  // March
  EXPECT_EQ(tm.tm_mday, 8);     // 8th
  EXPECT_EQ(tm.tm_hour, 7);     // 7:00 UTC = 2:00 EST
}

TEST(PosixTzParser, DstEndUSEastern2026) {
  // November 1, 2026 is 1st Sunday of November
  ParsedTimezone tz;
  parse_posix_tz("EST5EDT,M3.2.0/2,M11.1.0/2", tz);

  time_t dst_end = calculate_dst_transition(2026, tz.dst_end, tz.dst_offset_seconds);
  struct tm tm;
  internal::epoch_to_tm_utc(dst_end, &tm);

  // At 2:00 AM EDT (UTC-4), so 6:00 AM UTC
  EXPECT_EQ(tm.tm_year + 1900, 2026);
  EXPECT_EQ(tm.tm_mon + 1, 11);  // November
  EXPECT_EQ(tm.tm_mday, 1);      // 1st
  EXPECT_EQ(tm.tm_hour, 6);      // 6:00 UTC = 2:00 EDT
}

TEST(PosixTzParser, LastSundayOfMarch2026) {
  // Europe: M3.5.0 = last Sunday of March = March 29, 2026
  DSTRule rule{};
  rule.type = DSTRuleType::MONTH_WEEK_DAY;
  rule.month = 3;
  rule.week = 5;
  rule.day_of_week = 0;
  rule.time_seconds = 2 * 3600;
  time_t transition = calculate_dst_transition(2026, rule, 0);
  struct tm tm;
  internal::epoch_to_tm_utc(transition, &tm);
  EXPECT_EQ(tm.tm_mday, 29);
  EXPECT_EQ(tm.tm_wday, 0);  // Sunday
}

TEST(PosixTzParser, LastSundayOfOctober2026) {
  // Europe: M10.5.0 = last Sunday of October = October 25, 2026
  DSTRule rule{};
  rule.type = DSTRuleType::MONTH_WEEK_DAY;
  rule.month = 10;
  rule.week = 5;
  rule.day_of_week = 0;
  rule.time_seconds = 3 * 3600;
  time_t transition = calculate_dst_transition(2026, rule, 0);
  struct tm tm;
  internal::epoch_to_tm_utc(transition, &tm);
  EXPECT_EQ(tm.tm_mday, 25);
  EXPECT_EQ(tm.tm_wday, 0);  // Sunday
}

TEST(PosixTzParser, FirstSundayOfApril2026) {
  // April 5, 2026 is 1st Sunday
  DSTRule rule{};
  rule.type = DSTRuleType::MONTH_WEEK_DAY;
  rule.month = 4;
  rule.week = 1;
  rule.day_of_week = 0;
  rule.time_seconds = 0;
  time_t transition = calculate_dst_transition(2026, rule, 0);
  struct tm tm;
  internal::epoch_to_tm_utc(transition, &tm);
  EXPECT_EQ(tm.tm_mday, 5);
  EXPECT_EQ(tm.tm_wday, 0);
}

// ============================================================================
// DST detection tests
// ============================================================================

TEST(PosixTzParser, IsInDstUSEasternSummer) {
  ParsedTimezone tz;
  parse_posix_tz("EST5EDT,M3.2.0/2,M11.1.0/2", tz);

  // July 4, 2026 12:00 UTC - definitely in DST
  time_t summer = make_utc(2026, 7, 4, 12);
  EXPECT_TRUE(is_in_dst(summer, tz));
}

TEST(PosixTzParser, IsInDstUSEasternWinter) {
  ParsedTimezone tz;
  parse_posix_tz("EST5EDT,M3.2.0/2,M11.1.0/2", tz);

  // January 15, 2026 12:00 UTC - definitely not in DST
  time_t winter = make_utc(2026, 1, 15, 12);
  EXPECT_FALSE(is_in_dst(winter, tz));
}

TEST(PosixTzParser, IsInDstNoDstTimezone) {
  ParsedTimezone tz;
  parse_posix_tz("IST-5:30", tz);

  // July 15, 2026 12:00 UTC
  time_t epoch = make_utc(2026, 7, 15, 12);
  EXPECT_FALSE(is_in_dst(epoch, tz));
}

TEST(PosixTzParser, SouthernHemisphereDstSummer) {
  ParsedTimezone tz;
  parse_posix_tz("NZST-12NZDT,M9.5.0,M4.1.0/3", tz);

  // December 15, 2025 12:00 UTC - summer in NZ, should be in DST
  time_t nz_summer = make_utc(2025, 12, 15, 12);
  EXPECT_TRUE(is_in_dst(nz_summer, tz));
}

TEST(PosixTzParser, SouthernHemisphereDstWinter) {
  ParsedTimezone tz;
  parse_posix_tz("NZST-12NZDT,M9.5.0,M4.1.0/3", tz);

  // July 15, 2026 12:00 UTC - winter in NZ, should NOT be in DST
  time_t nz_winter = make_utc(2026, 7, 15, 12);
  EXPECT_FALSE(is_in_dst(nz_winter, tz));
}

// ============================================================================
// epoch_to_local_tm tests
// ============================================================================

TEST(PosixTzParser, EpochToLocalBasic) {
  ParsedTimezone tz;
  parse_posix_tz("UTC0", tz);

  time_t epoch = 0;  // Jan 1, 1970 00:00:00 UTC
  struct tm local;
  ASSERT_TRUE(epoch_to_local_tm(epoch, tz, &local));
  EXPECT_EQ(local.tm_year, 70);
  EXPECT_EQ(local.tm_mon, 0);
  EXPECT_EQ(local.tm_mday, 1);
  EXPECT_EQ(local.tm_hour, 0);
}

TEST(PosixTzParser, EpochToLocalWithOffset) {
  ParsedTimezone tz;
  parse_posix_tz("EST5", tz);  // UTC-5

  // Jan 1, 2026 05:00:00 UTC should be Jan 1, 2026 00:00:00 EST
  time_t utc_epoch = make_utc(2026, 1, 1, 5);

  struct tm local;
  ASSERT_TRUE(epoch_to_local_tm(utc_epoch, tz, &local));
  EXPECT_EQ(local.tm_hour, 0);  // Midnight EST
  EXPECT_EQ(local.tm_mday, 1);
  EXPECT_EQ(local.tm_isdst, 0);
}

TEST(PosixTzParser, EpochToLocalDstTransition) {
  ParsedTimezone tz;
  parse_posix_tz("EST5EDT,M3.2.0/2,M11.1.0/2", tz);

  // July 4, 2026 16:00 UTC = 12:00 EDT (noon)
  time_t utc_epoch = make_utc(2026, 7, 4, 16);

  struct tm local;
  ASSERT_TRUE(epoch_to_local_tm(utc_epoch, tz, &local));
  EXPECT_EQ(local.tm_hour, 12);  // Noon EDT
  EXPECT_EQ(local.tm_isdst, 1);
}

// ============================================================================
// Verification against libc
// ============================================================================

class LibcVerificationTest : public ::testing::TestWithParam<std::tuple<const char *, time_t>> {};

TEST_P(LibcVerificationTest, MatchesLibc) {
  auto [tz_str, epoch] = GetParam();

  ParsedTimezone tz;
  ASSERT_TRUE(parse_posix_tz(tz_str, tz));

  // Our implementation
  struct tm our_tm;
  epoch_to_local_tm(epoch, tz, &our_tm);

  // libc implementation
  setenv("TZ", tz_str, 1);
  tzset();
  struct tm *libc_tm = localtime(&epoch);

  EXPECT_EQ(our_tm.tm_year, libc_tm->tm_year);
  EXPECT_EQ(our_tm.tm_mon, libc_tm->tm_mon);
  EXPECT_EQ(our_tm.tm_mday, libc_tm->tm_mday);
  EXPECT_EQ(our_tm.tm_hour, libc_tm->tm_hour);
  EXPECT_EQ(our_tm.tm_min, libc_tm->tm_min);
  EXPECT_EQ(our_tm.tm_sec, libc_tm->tm_sec);
  EXPECT_EQ(our_tm.tm_isdst, libc_tm->tm_isdst);
}

INSTANTIATE_TEST_SUITE_P(USEastern, LibcVerificationTest,
                         ::testing::Values(std::make_tuple("EST5EDT,M3.2.0/2,M11.1.0/2", 1704067200),
                                           std::make_tuple("EST5EDT,M3.2.0/2,M11.1.0/2", 1720000000),
                                           std::make_tuple("EST5EDT,M3.2.0/2,M11.1.0/2", 1735689600)));

INSTANTIATE_TEST_SUITE_P(AngleBracket, LibcVerificationTest,
                         ::testing::Values(std::make_tuple("<+07>-7", 1704067200),
                                           std::make_tuple("<+07>-7", 1720000000)));

INSTANTIATE_TEST_SUITE_P(India, LibcVerificationTest,
                         ::testing::Values(std::make_tuple("IST-5:30", 1704067200),
                                           std::make_tuple("IST-5:30", 1720000000)));

INSTANTIATE_TEST_SUITE_P(NewZealand, LibcVerificationTest,
                         ::testing::Values(std::make_tuple("NZST-12NZDT,M9.5.0,M4.1.0/3", 1704067200),
                                           std::make_tuple("NZST-12NZDT,M9.5.0,M4.1.0/3", 1720000000)));

INSTANTIATE_TEST_SUITE_P(USCentral, LibcVerificationTest,
                         ::testing::Values(std::make_tuple("CST6CDT,M3.2.0/2,M11.1.0/2", 1704067200),
                                           std::make_tuple("CST6CDT,M3.2.0/2,M11.1.0/2", 1720000000),
                                           std::make_tuple("CST6CDT,M3.2.0/2,M11.1.0/2", 1735689600)));

INSTANTIATE_TEST_SUITE_P(EuropeBerlin, LibcVerificationTest,
                         ::testing::Values(std::make_tuple("CET-1CEST,M3.5.0,M10.5.0/3", 1704067200),
                                           std::make_tuple("CET-1CEST,M3.5.0,M10.5.0/3", 1720000000),
                                           std::make_tuple("CET-1CEST,M3.5.0,M10.5.0/3", 1735689600)));

INSTANTIATE_TEST_SUITE_P(AustraliaSydney, LibcVerificationTest,
                         ::testing::Values(std::make_tuple("AEST-10AEDT,M10.1.0,M4.1.0/3", 1704067200),
                                           std::make_tuple("AEST-10AEDT,M10.1.0,M4.1.0/3", 1720000000),
                                           std::make_tuple("AEST-10AEDT,M10.1.0,M4.1.0/3", 1735689600)));

// ============================================================================
// DST boundary edge cases
// ============================================================================

TEST(PosixTzParser, DstBoundaryJustBeforeSpringForward) {
  // Test 1 second before DST starts
  ParsedTimezone tz;
  parse_posix_tz("EST5EDT,M3.2.0/2,M11.1.0/2", tz);

  // March 8, 2026 06:59:59 UTC = 01:59:59 EST (1 second before spring forward)
  time_t before_epoch = make_utc(2026, 3, 8, 6, 59, 59);
  EXPECT_FALSE(is_in_dst(before_epoch, tz));

  // March 8, 2026 07:00:00 UTC = 02:00:00 EST -> 03:00:00 EDT (DST started)
  time_t after_epoch = make_utc(2026, 3, 8, 7);
  EXPECT_TRUE(is_in_dst(after_epoch, tz));
}

TEST(PosixTzParser, DstBoundaryJustBeforeFallBack) {
  // Test 1 second before DST ends
  ParsedTimezone tz;
  parse_posix_tz("EST5EDT,M3.2.0/2,M11.1.0/2", tz);

  // November 1, 2026 05:59:59 UTC = 01:59:59 EDT (1 second before fall back)
  time_t before_epoch = make_utc(2026, 11, 1, 5, 59, 59);
  EXPECT_TRUE(is_in_dst(before_epoch, tz));

  // November 1, 2026 06:00:00 UTC = 02:00:00 EDT -> 01:00:00 EST (DST ended)
  time_t after_epoch = make_utc(2026, 11, 1, 6);
  EXPECT_FALSE(is_in_dst(after_epoch, tz));
}

}  // namespace esphome::time::testing
