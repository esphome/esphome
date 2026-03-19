// Tests for the POSIX TZ parser, time conversion functions, and ESPTime::strptime.
//
// Most tests here cover the C++ POSIX TZ string parser (parse_posix_tz), which is
// bridge code for backward compatibility — it will be removed before ESPHome 2026.9.0.
// After https://github.com/esphome/esphome/pull/14233 merges, the parser is solely
// used to handle timezone strings from Home Assistant clients older than 2026.3.0
// that haven't been updated to send the pre-parsed ParsedTimezone protobuf struct.
// See https://github.com/esphome/backlog/issues/91
//
// The epoch_to_local_tm, is_in_dst, and ESPTime::strptime tests cover conversion
// functions that will remain permanently.

// Enable USE_TIME_TIMEZONE for tests
#define USE_TIME_TIMEZONE

#include <gtest/gtest.h>
#include <cstdlib>
#include <ctime>
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

// ============================================================================
// Basic TZ string parsing tests
// ============================================================================

TEST(PosixTzParser, ParseSimpleOffsetEST5) {
  ParsedTimezone tz;
  ASSERT_TRUE(parse_posix_tz("EST5", tz));
  EXPECT_EQ(tz.std_offset_seconds, 5 * 3600);  // +5 hours (west of UTC)
  EXPECT_FALSE(tz.has_dst());
}

TEST(PosixTzParser, ParseNegativeOffsetCET) {
  ParsedTimezone tz;
  ASSERT_TRUE(parse_posix_tz("CET-1", tz));
  EXPECT_EQ(tz.std_offset_seconds, -1 * 3600);  // -1 hour (east of UTC)
  EXPECT_FALSE(tz.has_dst());
}

TEST(PosixTzParser, ParseExplicitPositiveOffset) {
  ParsedTimezone tz;
  ASSERT_TRUE(parse_posix_tz("TEST+5", tz));
  EXPECT_EQ(tz.std_offset_seconds, 5 * 3600);
  EXPECT_FALSE(tz.has_dst());
}

TEST(PosixTzParser, ParseZeroOffset) {
  ParsedTimezone tz;
  ASSERT_TRUE(parse_posix_tz("UTC0", tz));
  EXPECT_EQ(tz.std_offset_seconds, 0);
  EXPECT_FALSE(tz.has_dst());
}

TEST(PosixTzParser, ParseUSEasternWithDST) {
  ParsedTimezone tz;
  ASSERT_TRUE(parse_posix_tz("EST5EDT,M3.2.0,M11.1.0", tz));
  EXPECT_EQ(tz.std_offset_seconds, 5 * 3600);
  EXPECT_EQ(tz.dst_offset_seconds, 4 * 3600);  // Default: STD - 1hr
  EXPECT_TRUE(tz.has_dst());
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
  EXPECT_TRUE(tz.has_dst());
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
  EXPECT_TRUE(tz.has_dst());
  EXPECT_EQ(tz.dst_start.month, 9);  // September
  EXPECT_EQ(tz.dst_end.month, 4);    // April
}

TEST(PosixTzParser, ParseExplicitDstOffset) {
  ParsedTimezone tz;
  // Some places have non-standard DST offsets
  ASSERT_TRUE(parse_posix_tz("TEST5DST4,M3.2.0,M11.1.0", tz));
  EXPECT_EQ(tz.std_offset_seconds, 5 * 3600);
  EXPECT_EQ(tz.dst_offset_seconds, 4 * 3600);
  EXPECT_TRUE(tz.has_dst());
}

// ============================================================================
// Angle-bracket notation tests (espressif/newlib-esp32#8)
// ============================================================================

TEST(PosixTzParser, ParseAngleBracketPositive) {
  // Format: <+07>-7 means UTC+7 (name is "+07", offset is -7 hours east)
  ParsedTimezone tz;
  ASSERT_TRUE(parse_posix_tz("<+07>-7", tz));
  EXPECT_EQ(tz.std_offset_seconds, -7 * 3600);  // -7 = 7 hours east of UTC
  EXPECT_FALSE(tz.has_dst());
}

TEST(PosixTzParser, ParseAngleBracketNegative) {
  // <-03>3 means UTC-3 (name is "-03", offset is 3 hours west)
  ParsedTimezone tz;
  ASSERT_TRUE(parse_posix_tz("<-03>3", tz));
  EXPECT_EQ(tz.std_offset_seconds, 3 * 3600);
  EXPECT_FALSE(tz.has_dst());
}

TEST(PosixTzParser, ParseAngleBracketWithDST) {
  // <+10>-10<+11>,M10.1.0,M4.1.0/3 (Australia/Sydney style)
  ParsedTimezone tz;
  ASSERT_TRUE(parse_posix_tz("<+10>-10<+11>,M10.1.0,M4.1.0/3", tz));
  EXPECT_EQ(tz.std_offset_seconds, -10 * 3600);
  EXPECT_EQ(tz.dst_offset_seconds, -11 * 3600);
  EXPECT_TRUE(tz.has_dst());
  EXPECT_EQ(tz.dst_start.month, 10);
  EXPECT_EQ(tz.dst_end.month, 4);
}

TEST(PosixTzParser, ParseAngleBracketNamed) {
  // <AEST>-10 (Australian Eastern Standard Time)
  ParsedTimezone tz;
  ASSERT_TRUE(parse_posix_tz("<AEST>-10", tz));
  EXPECT_EQ(tz.std_offset_seconds, -10 * 3600);
  EXPECT_FALSE(tz.has_dst());
}

TEST(PosixTzParser, ParseAngleBracketWithMinutes) {
  // <+0545>-5:45 (Nepal)
  ParsedTimezone tz;
  ASSERT_TRUE(parse_posix_tz("<+0545>-5:45", tz));
  EXPECT_EQ(tz.std_offset_seconds, -(5 * 3600 + 45 * 60));
  EXPECT_FALSE(tz.has_dst());
}

// ============================================================================
// Half-hour and unusual offset tests
// ============================================================================

TEST(PosixTzParser, ParseOffsetWithMinutesIndia) {
  ParsedTimezone tz;
  // India: UTC+5:30
  ASSERT_TRUE(parse_posix_tz("IST-5:30", tz));
  EXPECT_EQ(tz.std_offset_seconds, -(5 * 3600 + 30 * 60));
  EXPECT_FALSE(tz.has_dst());
}

TEST(PosixTzParser, ParseOffsetWithMinutesNepal) {
  ParsedTimezone tz;
  // Nepal: UTC+5:45
  ASSERT_TRUE(parse_posix_tz("NPT-5:45", tz));
  EXPECT_EQ(tz.std_offset_seconds, -(5 * 3600 + 45 * 60));
  EXPECT_FALSE(tz.has_dst());
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
  EXPECT_TRUE(tz.has_dst());
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
  EXPECT_TRUE(tz.has_dst());
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
  EXPECT_TRUE(tz.has_dst());
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
// Transition time edge cases (POSIX V3 allows -167 to +167 hours)
// ============================================================================

TEST(PosixTzParser, NegativeTransitionTime) {
  ParsedTimezone tz;
  // Negative transition time: /-1 means 11 PM (23:00) the previous day
  ASSERT_TRUE(parse_posix_tz("EST5EDT,M3.2.0/-1,M11.1.0/2", tz));
  EXPECT_EQ(tz.dst_start.time_seconds, -1 * 3600);  // -1 hour = 11 PM previous day
  EXPECT_EQ(tz.dst_end.time_seconds, 2 * 3600);
}

TEST(PosixTzParser, NegativeTransitionTimeWithMinutes) {
  ParsedTimezone tz;
  // /-1:30 means 10:30 PM the previous day
  ASSERT_TRUE(parse_posix_tz("EST5EDT,M3.2.0/-1:30,M11.1.0", tz));
  EXPECT_EQ(tz.dst_start.time_seconds, -(1 * 3600 + 30 * 60));
}

TEST(PosixTzParser, LargeTransitionTime) {
  ParsedTimezone tz;
  // POSIX V3 allows transition times from -167 to +167 hours
  // /25 means 1:00 AM the next day
  ASSERT_TRUE(parse_posix_tz("EST5EDT,M3.2.0/25,M11.1.0", tz));
  EXPECT_EQ(tz.dst_start.time_seconds, 25 * 3600);
}

TEST(PosixTzParser, MaxTransitionTime167Hours) {
  ParsedTimezone tz;
  // Maximum allowed transition time per POSIX V3
  ASSERT_TRUE(parse_posix_tz("EST5EDT,M3.2.0/167,M11.1.0", tz));
  EXPECT_EQ(tz.dst_start.time_seconds, 167 * 3600);
}

TEST(PosixTzParser, TransitionTimeWithHoursMinutesSeconds) {
  ParsedTimezone tz;
  ASSERT_TRUE(parse_posix_tz("EST5EDT,M3.2.0/2:30:45,M11.1.0", tz));
  EXPECT_EQ(tz.dst_start.time_seconds, 2 * 3600 + 30 * 60 + 45);
}

// ============================================================================
// Invalid M format tests
// ============================================================================

TEST(PosixTzParser, MFormatInvalidMonth13) {
  ParsedTimezone tz;
  // Month must be 1-12
  EXPECT_FALSE(parse_posix_tz("EST5EDT,M13.1.0,M11.1.0", tz));
}

TEST(PosixTzParser, MFormatInvalidMonth0) {
  ParsedTimezone tz;
  // Month must be 1-12
  EXPECT_FALSE(parse_posix_tz("EST5EDT,M0.1.0,M11.1.0", tz));
}

TEST(PosixTzParser, MFormatInvalidWeek6) {
  ParsedTimezone tz;
  // Week must be 1-5
  EXPECT_FALSE(parse_posix_tz("EST5EDT,M3.6.0,M11.1.0", tz));
}

TEST(PosixTzParser, MFormatInvalidWeek0) {
  ParsedTimezone tz;
  // Week must be 1-5
  EXPECT_FALSE(parse_posix_tz("EST5EDT,M3.0.0,M11.1.0", tz));
}

TEST(PosixTzParser, MFormatInvalidDayOfWeek7) {
  ParsedTimezone tz;
  // Day of week must be 0-6
  EXPECT_FALSE(parse_posix_tz("EST5EDT,M3.2.7,M11.1.0", tz));
}

TEST(PosixTzParser, MissingEndRule) {
  ParsedTimezone tz;
  // POSIX requires both start and end rules if any rules are specified
  EXPECT_FALSE(parse_posix_tz("EST5EDT,M3.2.0", tz));
}

TEST(PosixTzParser, MissingEndRuleJFormat) {
  ParsedTimezone tz;
  // POSIX requires both start and end rules if any rules are specified
  EXPECT_FALSE(parse_posix_tz("EST5EDT,J60", tz));
}

TEST(PosixTzParser, MissingEndRulePlainDay) {
  ParsedTimezone tz;
  // POSIX requires both start and end rules if any rules are specified
  EXPECT_FALSE(parse_posix_tz("EST5EDT,60", tz));
}

TEST(PosixTzParser, LowercaseMFormat) {
  ParsedTimezone tz;
  // Lowercase 'm' should be accepted
  ASSERT_TRUE(parse_posix_tz("EST5EDT,m3.2.0,m11.1.0", tz));
  EXPECT_TRUE(tz.has_dst());
  EXPECT_EQ(tz.dst_start.month, 3);
  EXPECT_EQ(tz.dst_end.month, 11);
}

TEST(PosixTzParser, LowercaseJFormat) {
  ParsedTimezone tz;
  // Lowercase 'j' should be accepted
  ASSERT_TRUE(parse_posix_tz("EST5EDT,j60,j305", tz));
  EXPECT_EQ(tz.dst_start.type, DSTRuleType::JULIAN_NO_LEAP);
  EXPECT_EQ(tz.dst_start.day, 60);
}

TEST(PosixTzParser, DstNameWithoutRules) {
  ParsedTimezone tz;
  // DST name present but no rules - treat as no DST since we can't determine transitions
  ASSERT_TRUE(parse_posix_tz("EST5EDT", tz));
  EXPECT_FALSE(tz.has_dst());
  EXPECT_EQ(tz.std_offset_seconds, 5 * 3600);
}

TEST(PosixTzParser, TrailingCharactersIgnored) {
  ParsedTimezone tz;
  // Trailing characters after valid TZ should be ignored (parser stops at end of valid input)
  // This matches libc behavior
  ASSERT_TRUE(parse_posix_tz("EST5 extra garbage here", tz));
  EXPECT_EQ(tz.std_offset_seconds, 5 * 3600);
  EXPECT_FALSE(tz.has_dst());
}

TEST(PosixTzParser, PlainDay365LeapYear) {
  // Day 365 in leap year is Dec 31
  int month, day;
  internal::day_of_year_to_month_day(365, 2024, month, day);
  EXPECT_EQ(month, 12);
  EXPECT_EQ(day, 31);
}

TEST(PosixTzParser, PlainDay364NonLeapYear) {
  // Day 364 (0-indexed) is Dec 31 in non-leap year (last valid day)
  int month, day;
  internal::day_of_year_to_month_day(364, 2025, month, day);
  EXPECT_EQ(month, 12);
  EXPECT_EQ(day, 31);
}

// ============================================================================
// Large offset tests
// ============================================================================

TEST(PosixTzParser, MaxOffset14Hours) {
  ParsedTimezone tz;
  // Line Islands (Kiribati) is UTC+14, the maximum offset
  ASSERT_TRUE(parse_posix_tz("<+14>-14", tz));
  EXPECT_EQ(tz.std_offset_seconds, -14 * 3600);
}

TEST(PosixTzParser, MaxNegativeOffset12Hours) {
  ParsedTimezone tz;
  // Baker Island is UTC-12
  ASSERT_TRUE(parse_posix_tz("<-12>12", tz));
  EXPECT_EQ(tz.std_offset_seconds, 12 * 3600);
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

TEST(PosixTzParser, JulianDay31IsJan31) {
  int month, day;
  internal::julian_to_month_day(31, month, day);
  EXPECT_EQ(month, 1);
  EXPECT_EQ(day, 31);
}

TEST(PosixTzParser, JulianDay32IsFeb1) {
  int month, day;
  internal::julian_to_month_day(32, month, day);
  EXPECT_EQ(month, 2);
  EXPECT_EQ(day, 1);
}

TEST(PosixTzParser, JulianDay59IsFeb28) {
  int month, day;
  internal::julian_to_month_day(59, month, day);
  EXPECT_EQ(month, 2);
  EXPECT_EQ(day, 28);
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

  time_t dst_start = internal::calculate_dst_transition(2026, tz.dst_start, tz.std_offset_seconds);
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

  time_t dst_end = internal::calculate_dst_transition(2026, tz.dst_end, tz.dst_offset_seconds);
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
  time_t transition = internal::calculate_dst_transition(2026, rule, 0);
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
  time_t transition = internal::calculate_dst_transition(2026, rule, 0);
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
  time_t transition = internal::calculate_dst_transition(2026, rule, 0);
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

TEST(PosixTzParser, EpochToLocalNegativeEpoch) {
  ParsedTimezone tz;
  parse_posix_tz("UTC0", tz);

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

TEST(PosixTzParser, EpochToLocalNullTmFails) {
  ParsedTimezone tz;
  parse_posix_tz("UTC0", tz);
  EXPECT_FALSE(epoch_to_local_tm(0, tz, nullptr));
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

class LibcVerificationTest : public ::testing::TestWithParam<std::tuple<const char *, time_t>> {
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
  auto [tz_str, epoch] = GetParam();

  ParsedTimezone tz;
  ASSERT_TRUE(parse_posix_tz(tz_str, tz));

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
  // Full datetime with extra characters should fail
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
  t.day_of_week = 1;  // Placeholder for fields_in_range()
  t.day_of_year = 1;
  t.recalc_timestamp_local();
  return t.timestamp;
}

TEST(RecalcTimestampLocal, NormalTimeMatchesLibc) {
  // Set timezone to US Central (CST6CDT)
  const char *tz_str = "CST6CDT,M3.2.0,M11.1.0";
  setenv("TZ", tz_str, 1);
  tzset();
  time::ParsedTimezone tz{};
  ASSERT_TRUE(parse_posix_tz(tz_str, tz));
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
  time::ParsedTimezone tz{};
  ASSERT_TRUE(parse_posix_tz(tz_str, tz));
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
  time::ParsedTimezone tz{};
  ASSERT_TRUE(parse_posix_tz(tz_str, tz));
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
  // Set timezone to Australia/Sydney (AEST-10AEDT,M10.1.0,M4.1.0)
  // DST starts first Sunday of October, ends first Sunday of April
  const char *tz_str = "AEST-10AEDT,M10.1.0,M4.1.0";
  setenv("TZ", tz_str, 1);
  tzset();
  time::ParsedTimezone tz{};
  ASSERT_TRUE(parse_posix_tz(tz_str, tz));
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
  time::ParsedTimezone tz{};
  ASSERT_TRUE(parse_posix_tz(tz_str, tz));
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
  time::ParsedTimezone tz{};
  ASSERT_TRUE(parse_posix_tz(tz_str, tz));
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

TEST(RecalcTimestampLocal, YearBoundaryDST) {
  // Test southern hemisphere DST across year boundary
  // Australia/Sydney: DST active from October to April (spans Jan 1)
  const char *tz_str = "AEST-10AEDT,M10.1.0,M4.1.0";
  setenv("TZ", tz_str, 1);
  tzset();
  time::ParsedTimezone tz{};
  ASSERT_TRUE(parse_posix_tz(tz_str, tz));
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
  // When no timezone is set, offset should be 0
  time::ParsedTimezone tz{};
  set_global_tz(tz);

  int32_t offset = ESPTime::timezone_offset();
  EXPECT_EQ(offset, 0);
}

TEST(TimezoneOffset, FixedOffsetPositive) {
  // India: UTC+5:30 (no DST)
  const char *tz_str = "IST-5:30";
  time::ParsedTimezone tz{};
  ASSERT_TRUE(parse_posix_tz(tz_str, tz));
  set_global_tz(tz);

  int32_t offset = ESPTime::timezone_offset();
  // Offset should be +5:30 = 19800 seconds (to add to UTC to get local)
  EXPECT_EQ(offset, 5 * 3600 + 30 * 60);
}

TEST(TimezoneOffset, FixedOffsetNegative) {
  // US Eastern Standard Time: UTC-5 (testing without DST rules)
  const char *tz_str = "EST5";
  time::ParsedTimezone tz{};
  ASSERT_TRUE(parse_posix_tz(tz_str, tz));
  set_global_tz(tz);

  int32_t offset = ESPTime::timezone_offset();
  // Offset should be -5 hours = -18000 seconds
  EXPECT_EQ(offset, -5 * 3600);
}

TEST(TimezoneOffset, WithDstReturnsCorrectOffsetBasedOnCurrentTime) {
  // US Eastern with DST
  const char *tz_str = "EST5EDT,M3.2.0,M11.1.0";
  time::ParsedTimezone tz{};
  ASSERT_TRUE(parse_posix_tz(tz_str, tz));
  set_global_tz(tz);

  // Get current time and check offset matches expected based on DST status
  time_t now = ::time(nullptr);
  int32_t offset = ESPTime::timezone_offset();

  // Verify offset matches what is_in_dst says
  if (time::is_in_dst(now, tz)) {
    // During DST, offset should be -4 hours (EDT)
    EXPECT_EQ(offset, -4 * 3600);
  } else {
    // During standard time, offset should be -5 hours (EST)
    EXPECT_EQ(offset, -5 * 3600);
  }
}

}  // namespace esphome::testing
