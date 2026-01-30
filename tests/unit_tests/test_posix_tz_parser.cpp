// Test POSIX TZ parser implementation
// Compile with: g++ -std=gnu++20 -I../../esphome/core -o test_posix_tz_parser test_posix_tz_parser.cpp
// ../../esphome/core/posix_tz.cpp && ./test_posix_tz_parser
//
// This test verifies our custom POSIX TZ parser produces identical results to libc's
// tzset()/localtime() implementation. The custom parser avoids pulling in scanf (~7.6KB).
//
// Key test cases include:
// - Angle-bracket timezone notation (<+07>-7) - see espressif/newlib-esp32#8
// - Half-hour offsets (IST-5:30)
// - Southern hemisphere DST (start month > end month)
// - DST transition boundary conditions

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

// Include the implementation directly for standalone compilation
#include "../../esphome/core/posix_tz.h"
#include "../../esphome/core/posix_tz.cpp"

using namespace esphome;

#define TEST(name) static void test_##name()
#define RUN_TEST(name) \
  do { \
    printf("  " #name "..."); \
    fflush(stdout); \
    test_##name(); \
    printf(" OK\n"); \
  } while (0)

// ============================================================================
// Basic TZ string parsing tests
// ============================================================================

TEST(parse_simple_offset_est5) {
  ParsedTimezone tz;
  assert(parse_posix_tz("EST5", tz));
  assert(tz.std_offset_seconds == 5 * 3600);  // +5 hours (west of UTC)
  assert(!tz.has_dst);
}

TEST(parse_negative_offset_cet) {
  ParsedTimezone tz;
  assert(parse_posix_tz("CET-1", tz));
  assert(tz.std_offset_seconds == -1 * 3600);  // -1 hour (east of UTC)
  assert(!tz.has_dst);
}

TEST(parse_explicit_positive_offset) {
  ParsedTimezone tz;
  assert(parse_posix_tz("TEST+5", tz));
  assert(tz.std_offset_seconds == 5 * 3600);
  assert(!tz.has_dst);
}

TEST(parse_zero_offset) {
  ParsedTimezone tz;
  assert(parse_posix_tz("UTC0", tz));
  assert(tz.std_offset_seconds == 0);
  assert(!tz.has_dst);
}

TEST(parse_us_eastern_with_dst) {
  ParsedTimezone tz;
  assert(parse_posix_tz("EST5EDT,M3.2.0,M11.1.0", tz));
  assert(tz.std_offset_seconds == 5 * 3600);
  assert(tz.dst_offset_seconds == 4 * 3600);  // Default: STD - 1hr
  assert(tz.has_dst);
  assert(tz.dst_start.month == 3);
  assert(tz.dst_start.week == 2);
  assert(tz.dst_start.day_of_week == 0);          // Sunday
  assert(tz.dst_start.time_seconds == 2 * 3600);  // Default 2:00 AM
  assert(tz.dst_end.month == 11);
  assert(tz.dst_end.week == 1);
  assert(tz.dst_end.day_of_week == 0);
}

TEST(parse_us_central_with_time) {
  ParsedTimezone tz;
  assert(parse_posix_tz("CST6CDT,M3.2.0/2,M11.1.0/2", tz));
  assert(tz.std_offset_seconds == 6 * 3600);
  assert(tz.dst_offset_seconds == 5 * 3600);
  assert(tz.dst_start.time_seconds == 2 * 3600);  // 2:00 AM
  assert(tz.dst_end.time_seconds == 2 * 3600);
}

TEST(parse_europe_berlin) {
  ParsedTimezone tz;
  assert(parse_posix_tz("CET-1CEST,M3.5.0,M10.5.0/3", tz));
  assert(tz.std_offset_seconds == -1 * 3600);
  assert(tz.dst_offset_seconds == -2 * 3600);  // Default: STD - 1hr
  assert(tz.has_dst);
  assert(tz.dst_start.month == 3);
  assert(tz.dst_start.week == 5);  // Last week
  assert(tz.dst_end.month == 10);
  assert(tz.dst_end.week == 5);                 // Last week
  assert(tz.dst_end.time_seconds == 3 * 3600);  // 3:00 AM
}

TEST(parse_new_zealand) {
  ParsedTimezone tz;
  // Southern hemisphere - DST starts in Sept, ends in April
  assert(parse_posix_tz("NZST-12NZDT,M9.5.0,M4.1.0/3", tz));
  assert(tz.std_offset_seconds == -12 * 3600);
  assert(tz.dst_offset_seconds == -13 * 3600);  // Default: STD - 1hr
  assert(tz.has_dst);
  assert(tz.dst_start.month == 9);  // September
  assert(tz.dst_end.month == 4);    // April
}

TEST(parse_explicit_dst_offset) {
  ParsedTimezone tz;
  // Some places have non-standard DST offsets
  assert(parse_posix_tz("TEST5DST4,M3.2.0,M11.1.0", tz));
  assert(tz.std_offset_seconds == 5 * 3600);
  assert(tz.dst_offset_seconds == 4 * 3600);
  assert(tz.has_dst);
}

// ============================================================================
// Angle-bracket notation tests (espressif/newlib-esp32#8)
// ============================================================================

TEST(parse_angle_bracket_positive) {
  // Format: <+07>-7 means UTC+7 (name is "+07", offset is -7 hours east)
  ParsedTimezone tz;
  assert(parse_posix_tz("<+07>-7", tz));
  assert(tz.std_offset_seconds == -7 * 3600);  // -7 = 7 hours east of UTC
  assert(!tz.has_dst);
}

TEST(parse_angle_bracket_negative) {
  // <-03>3 means UTC-3 (name is "-03", offset is 3 hours west)
  ParsedTimezone tz;
  assert(parse_posix_tz("<-03>3", tz));
  assert(tz.std_offset_seconds == 3 * 3600);
  assert(!tz.has_dst);
}

TEST(parse_angle_bracket_with_dst) {
  // <+10>-10<+11>,M10.1.0,M4.1.0/3 (Australia/Sydney style)
  ParsedTimezone tz;
  assert(parse_posix_tz("<+10>-10<+11>,M10.1.0,M4.1.0/3", tz));
  assert(tz.std_offset_seconds == -10 * 3600);
  assert(tz.dst_offset_seconds == -11 * 3600);
  assert(tz.has_dst);
  assert(tz.dst_start.month == 10);
  assert(tz.dst_end.month == 4);
}

TEST(parse_angle_bracket_named) {
  // <AEST>-10 (Australian Eastern Standard Time)
  ParsedTimezone tz;
  assert(parse_posix_tz("<AEST>-10", tz));
  assert(tz.std_offset_seconds == -10 * 3600);
  assert(!tz.has_dst);
}

TEST(parse_angle_bracket_with_minutes) {
  // <+0545>-5:45 (Nepal)
  ParsedTimezone tz;
  assert(parse_posix_tz("<+0545>-5:45", tz));
  assert(tz.std_offset_seconds == -(5 * 3600 + 45 * 60));
  assert(!tz.has_dst);
}

// ============================================================================
// Half-hour and unusual offset tests
// ============================================================================

TEST(parse_offset_with_minutes_india) {
  ParsedTimezone tz;
  // India: UTC+5:30
  assert(parse_posix_tz("IST-5:30", tz));
  assert(tz.std_offset_seconds == -(5 * 3600 + 30 * 60));
  assert(!tz.has_dst);
}

TEST(parse_offset_with_minutes_nepal) {
  ParsedTimezone tz;
  // Nepal: UTC+5:45
  assert(parse_posix_tz("NPT-5:45", tz));
  assert(tz.std_offset_seconds == -(5 * 3600 + 45 * 60));
  assert(!tz.has_dst);
}

TEST(parse_offset_with_seconds) {
  ParsedTimezone tz;
  // Unusual but valid: offset with seconds
  assert(parse_posix_tz("TEST-1:30:30", tz));
  assert(tz.std_offset_seconds == -(1 * 3600 + 30 * 60 + 30));
}

TEST(parse_chatham_islands) {
  // Chatham Islands: UTC+12:45 with DST
  ParsedTimezone tz;
  assert(parse_posix_tz("<+1245>-12:45<+1345>,M9.5.0/2:45,M4.1.0/3:45", tz));
  assert(tz.std_offset_seconds == -(12 * 3600 + 45 * 60));
  assert(tz.dst_offset_seconds == -(13 * 3600 + 45 * 60));
  assert(tz.has_dst);
}

// ============================================================================
// Invalid input tests
// ============================================================================

TEST(parse_empty_string_fails) {
  ParsedTimezone tz;
  assert(!parse_posix_tz("", tz));
}

TEST(parse_null_fails) {
  ParsedTimezone tz;
  assert(!parse_posix_tz(nullptr, tz));
}

TEST(parse_short_name_fails) {
  ParsedTimezone tz;
  // TZ name must be at least 3 characters
  assert(!parse_posix_tz("AB5", tz));
}

TEST(parse_missing_offset_fails) {
  ParsedTimezone tz;
  assert(!parse_posix_tz("EST", tz));
}

TEST(parse_unterminated_bracket_fails) {
  ParsedTimezone tz;
  assert(!parse_posix_tz("<+07-7", tz));  // Missing closing >
}

// ============================================================================
// J-format and plain day number tests
// ============================================================================

TEST(parse_j_format_basic) {
  ParsedTimezone tz;
  // J format: Julian day 1-365, not counting Feb 29
  assert(parse_posix_tz("EST5EDT,J60,J305", tz));
  assert(tz.has_dst);
  assert(tz.dst_start.type == DSTRuleType::JULIAN_NO_LEAP);
  assert(tz.dst_start.day == 60);  // March 1
  assert(tz.dst_end.type == DSTRuleType::JULIAN_NO_LEAP);
  assert(tz.dst_end.day == 305);  // November 1
}

TEST(parse_j_format_with_time) {
  ParsedTimezone tz;
  assert(parse_posix_tz("EST5EDT,J60/2,J305/2", tz));
  assert(tz.dst_start.day == 60);
  assert(tz.dst_start.time_seconds == 2 * 3600);
  assert(tz.dst_end.day == 305);
  assert(tz.dst_end.time_seconds == 2 * 3600);
}

TEST(parse_plain_day_number) {
  ParsedTimezone tz;
  // Plain format: day 0-365, counting Feb 29 in leap years
  assert(parse_posix_tz("EST5EDT,59,304", tz));
  assert(tz.has_dst);
  assert(tz.dst_start.type == DSTRuleType::DAY_OF_YEAR);
  assert(tz.dst_start.day == 59);  // Feb 29 or March 1 depending on leap year
  assert(tz.dst_end.type == DSTRuleType::DAY_OF_YEAR);
  assert(tz.dst_end.day == 304);
}

TEST(parse_plain_day_number_with_time) {
  ParsedTimezone tz;
  assert(parse_posix_tz("EST5EDT,59/3,304/1:30", tz));
  assert(tz.dst_start.day == 59);
  assert(tz.dst_start.time_seconds == 3 * 3600);
  assert(tz.dst_end.day == 304);
  assert(tz.dst_end.time_seconds == 1 * 3600 + 30 * 60);
}

TEST(j_format_invalid_day_zero) {
  ParsedTimezone tz;
  // J format day must be 1-365, not 0
  assert(!parse_posix_tz("EST5EDT,J0,J305", tz));
}

TEST(j_format_invalid_day_366) {
  ParsedTimezone tz;
  // J format day must be 1-365
  assert(!parse_posix_tz("EST5EDT,J366,J305", tz));
}

TEST(plain_day_invalid_day_366) {
  ParsedTimezone tz;
  // Plain format day must be 0-365
  assert(!parse_posix_tz("EST5EDT,366,304", tz));
}

// ============================================================================
// Julian day to month/day conversion tests
// ============================================================================

TEST(julian_day_60_is_march_1) {
  // J60 is always March 1, regardless of leap year
  int month, day;
  internal::julian_to_month_day(60, 2024, month, day);  // Leap year
  assert(month == 3 && day == 1);
  internal::julian_to_month_day(60, 2025, month, day);  // Non-leap year
  assert(month == 3 && day == 1);
}

TEST(julian_day_1_is_jan_1) {
  int month, day;
  internal::julian_to_month_day(1, 2025, month, day);
  assert(month == 1 && day == 1);
}

TEST(julian_day_365_is_dec_31) {
  int month, day;
  internal::julian_to_month_day(365, 2025, month, day);
  assert(month == 12 && day == 31);
}

TEST(day_of_year_59_differs_by_leap) {
  int month, day;
  // Day 59 in leap year is Feb 29
  internal::day_of_year_to_month_day(59, 2024, month, day);
  assert(month == 2 && day == 29);
  // Day 59 in non-leap year is March 1
  internal::day_of_year_to_month_day(59, 2025, month, day);
  assert(month == 3 && day == 1);
}

TEST(day_of_year_0_is_jan_1) {
  int month, day;
  internal::day_of_year_to_month_day(0, 2025, month, day);
  assert(month == 1 && day == 1);
}

// ============================================================================
// Day of week calculation tests
// ============================================================================

TEST(day_of_week_known_dates) {
  // January 1, 1970 was Thursday (4)
  assert(internal::day_of_week(1970, 1, 1) == 4);

  // July 4, 1776 was Thursday (4)
  assert(internal::day_of_week(1776, 7, 4) == 4);

  // January 1, 2000 was Saturday (6)
  assert(internal::day_of_week(2000, 1, 1) == 6);

  // September 11, 2001 was Tuesday (2)
  assert(internal::day_of_week(2001, 9, 11) == 2);

  // March 8, 2026 is Sunday (0) - US DST start
  assert(internal::day_of_week(2026, 3, 8) == 0);

  // November 1, 2026 is Sunday (0) - US DST end
  assert(internal::day_of_week(2026, 11, 1) == 0);
}

TEST(leap_year_detection) {
  assert(!internal::is_leap_year(1900));  // Divisible by 100 but not 400
  assert(internal::is_leap_year(2000));   // Divisible by 400
  assert(internal::is_leap_year(2024));   // Divisible by 4
  assert(!internal::is_leap_year(2025));  // Not divisible by 4
  assert(internal::is_leap_year(2028));
}

TEST(days_in_month_regular) {
  assert(internal::days_in_month(2025, 1) == 31);
  assert(internal::days_in_month(2025, 2) == 28);
  assert(internal::days_in_month(2025, 4) == 30);
  assert(internal::days_in_month(2025, 12) == 31);
}

TEST(days_in_month_leap_year) {
  assert(internal::days_in_month(2024, 2) == 29);
  assert(internal::days_in_month(2025, 2) == 28);
}

// ============================================================================
// DST transition calculation tests
// ============================================================================

TEST(dst_start_us_eastern_2026) {
  // March 8, 2026 is 2nd Sunday of March
  ParsedTimezone tz;
  parse_posix_tz("EST5EDT,M3.2.0/2,M11.1.0/2", tz);

  time_t dst_start = calculate_dst_transition(2026, tz.dst_start, tz.std_offset_seconds);
  struct tm tm;
  internal::epoch_to_tm_utc(dst_start, &tm);

  // At 2:00 AM EST (UTC-5), so 7:00 AM UTC
  assert(tm.tm_year + 1900 == 2026);
  assert(tm.tm_mon + 1 == 3);  // March
  assert(tm.tm_mday == 8);     // 8th
  assert(tm.tm_hour == 7);     // 7:00 UTC = 2:00 EST
}

TEST(dst_end_us_eastern_2026) {
  // November 1, 2026 is 1st Sunday of November
  ParsedTimezone tz;
  parse_posix_tz("EST5EDT,M3.2.0/2,M11.1.0/2", tz);

  time_t dst_end = calculate_dst_transition(2026, tz.dst_end, tz.dst_offset_seconds);
  struct tm tm;
  internal::epoch_to_tm_utc(dst_end, &tm);

  // At 2:00 AM EDT (UTC-4), so 6:00 AM UTC
  assert(tm.tm_year + 1900 == 2026);
  assert(tm.tm_mon + 1 == 11);  // November
  assert(tm.tm_mday == 1);      // 1st
  assert(tm.tm_hour == 6);      // 6:00 UTC = 2:00 EDT
}

TEST(last_sunday_of_march_2026) {
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
  assert(tm.tm_mday == 29);
  assert(tm.tm_wday == 0);  // Sunday
}

TEST(last_sunday_of_october_2026) {
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
  assert(tm.tm_mday == 25);
  assert(tm.tm_wday == 0);  // Sunday
}

TEST(first_sunday_of_april_2026) {
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
  assert(tm.tm_mday == 5);
  assert(tm.tm_wday == 0);
}

// ============================================================================
// is_in_dst tests
// ============================================================================

TEST(is_in_dst_us_eastern_summer) {
  ParsedTimezone tz;
  parse_posix_tz("EST5EDT,M3.2.0/2,M11.1.0/2", tz);

  // July 4, 2026 12:00 UTC - definitely in DST
  struct tm july4 {};
  july4.tm_sec = 0;
  july4.tm_min = 0;
  july4.tm_hour = 12;
  july4.tm_mday = 4;
  july4.tm_mon = 6;
  july4.tm_year = 126;
  time_t summer = internal::tm_to_epoch_utc(&july4);
  assert(is_in_dst(summer, tz) == true);
}

TEST(is_in_dst_us_eastern_winter) {
  ParsedTimezone tz;
  parse_posix_tz("EST5EDT,M3.2.0/2,M11.1.0/2", tz);

  // January 15, 2026 12:00 UTC - definitely not in DST
  struct tm jan15 {};
  jan15.tm_sec = 0;
  jan15.tm_min = 0;
  jan15.tm_hour = 12;
  jan15.tm_mday = 15;
  jan15.tm_mon = 0;
  jan15.tm_year = 126;
  time_t winter = internal::tm_to_epoch_utc(&jan15);
  assert(is_in_dst(winter, tz) == false);
}

TEST(is_in_dst_no_dst_timezone) {
  ParsedTimezone tz;
  parse_posix_tz("IST-5:30", tz);

  struct tm anytime {};
  anytime.tm_sec = 0;
  anytime.tm_min = 0;
  anytime.tm_hour = 12;
  anytime.tm_mday = 15;
  anytime.tm_mon = 6;
  anytime.tm_year = 126;
  time_t epoch = internal::tm_to_epoch_utc(&anytime);
  assert(is_in_dst(epoch, tz) == false);
}

TEST(southern_hemisphere_dst_summer) {
  ParsedTimezone tz;
  parse_posix_tz("NZST-12NZDT,M9.5.0,M4.1.0/3", tz);

  // December 15, 2025 12:00 UTC - summer in NZ, should be in DST
  struct tm dec15 {};
  dec15.tm_sec = 0;
  dec15.tm_min = 0;
  dec15.tm_hour = 12;
  dec15.tm_mday = 15;
  dec15.tm_mon = 11;
  dec15.tm_year = 125;
  time_t nz_summer = internal::tm_to_epoch_utc(&dec15);
  assert(is_in_dst(nz_summer, tz) == true);
}

TEST(southern_hemisphere_dst_winter) {
  ParsedTimezone tz;
  parse_posix_tz("NZST-12NZDT,M9.5.0,M4.1.0/3", tz);

  // July 15, 2026 12:00 UTC - winter in NZ, should NOT be in DST
  struct tm july15 {};
  july15.tm_sec = 0;
  july15.tm_min = 0;
  july15.tm_hour = 12;
  july15.tm_mday = 15;
  july15.tm_mon = 6;
  july15.tm_year = 126;
  time_t nz_winter = internal::tm_to_epoch_utc(&july15);
  assert(is_in_dst(nz_winter, tz) == false);
}

// ============================================================================
// epoch_to_local_tm tests
// ============================================================================

TEST(epoch_to_local_basic) {
  ParsedTimezone tz;
  parse_posix_tz("UTC0", tz);

  time_t epoch = 0;  // Jan 1, 1970 00:00:00 UTC
  struct tm local;
  assert(epoch_to_local_tm(epoch, tz, &local));
  assert(local.tm_year == 70);
  assert(local.tm_mon == 0);
  assert(local.tm_mday == 1);
  assert(local.tm_hour == 0);
}

TEST(epoch_to_local_with_offset) {
  ParsedTimezone tz;
  parse_posix_tz("EST5", tz);  // UTC-5

  // Jan 1, 2026 05:00:00 UTC should be Jan 1, 2026 00:00:00 EST
  struct tm utc_tm {};
  utc_tm.tm_sec = 0;
  utc_tm.tm_min = 0;
  utc_tm.tm_hour = 5;
  utc_tm.tm_mday = 1;
  utc_tm.tm_mon = 0;
  utc_tm.tm_year = 126;
  time_t utc_epoch = internal::tm_to_epoch_utc(&utc_tm);

  struct tm local;
  assert(epoch_to_local_tm(utc_epoch, tz, &local));
  assert(local.tm_hour == 0);  // Midnight EST
  assert(local.tm_mday == 1);
  assert(local.tm_isdst == 0);
}

TEST(epoch_to_local_dst_transition) {
  ParsedTimezone tz;
  parse_posix_tz("EST5EDT,M3.2.0/2,M11.1.0/2", tz);

  // July 4, 2026 16:00 UTC = 12:00 EDT (noon)
  struct tm july4_utc {};
  july4_utc.tm_sec = 0;
  july4_utc.tm_min = 0;
  july4_utc.tm_hour = 16;
  july4_utc.tm_mday = 4;
  july4_utc.tm_mon = 6;
  july4_utc.tm_year = 126;
  time_t utc_epoch = internal::tm_to_epoch_utc(&july4_utc);

  struct tm local;
  assert(epoch_to_local_tm(utc_epoch, tz, &local));
  assert(local.tm_hour == 12);  // Noon EDT
  assert(local.tm_isdst == 1);
}

// ============================================================================
// Verification against libc (run on desktop only)
// ============================================================================

// Helper to compare our implementation against libc
static bool verify_against_libc(const char *tz_str, time_t epoch) {
  ParsedTimezone tz;
  if (!parse_posix_tz(tz_str, tz)) {
    printf("Failed to parse TZ: %s\n", tz_str);
    return false;
  }

  // Our implementation
  struct tm our_tm;
  epoch_to_local_tm(epoch, tz, &our_tm);

  // libc implementation
  setenv("TZ", tz_str, 1);
  tzset();
  struct tm *libc_tm = localtime(&epoch);

  bool match =
      (our_tm.tm_year == libc_tm->tm_year && our_tm.tm_mon == libc_tm->tm_mon && our_tm.tm_mday == libc_tm->tm_mday &&
       our_tm.tm_hour == libc_tm->tm_hour && our_tm.tm_min == libc_tm->tm_min && our_tm.tm_sec == libc_tm->tm_sec &&
       our_tm.tm_isdst == libc_tm->tm_isdst);

  if (!match) {
    printf("\nMismatch for TZ=%s epoch=%ld\n", tz_str, (long) epoch);
    printf("  Our:  %04d-%02d-%02d %02d:%02d:%02d DST=%d\n", our_tm.tm_year + 1900, our_tm.tm_mon + 1, our_tm.tm_mday,
           our_tm.tm_hour, our_tm.tm_min, our_tm.tm_sec, our_tm.tm_isdst);
    printf("  libc: %04d-%02d-%02d %02d:%02d:%02d DST=%d\n", libc_tm->tm_year + 1900, libc_tm->tm_mon + 1,
           libc_tm->tm_mday, libc_tm->tm_hour, libc_tm->tm_min, libc_tm->tm_sec, libc_tm->tm_isdst);
  }

  return match;
}

TEST(verify_us_eastern_multiple_epochs) {
  const char *tz_str = "EST5EDT,M3.2.0/2,M11.1.0/2";
  // Test various dates throughout the year
  time_t epochs[] = {
      1704067200,  // Jan 1, 2024 00:00 UTC
      1711900800,  // March 31, 2024 12:00 UTC (after DST start)
      1720000000,  // July 3, 2024 (summer)
      1730419200,  // Nov 1, 2024 00:00 UTC (DST end day)
      1735689600,  // Jan 1, 2025 00:00 UTC
  };
  for (time_t epoch : epochs) {
    assert(verify_against_libc(tz_str, epoch));
  }
}

TEST(verify_us_central_multiple_epochs) {
  const char *tz_str = "CST6CDT,M3.2.0/2,M11.1.0/2";
  time_t epochs[] = {
      1704067200, 1711900800, 1720000000, 1730419200, 1735689600,
  };
  for (time_t epoch : epochs) {
    assert(verify_against_libc(tz_str, epoch));
  }
}

TEST(verify_europe_berlin_multiple_epochs) {
  const char *tz_str = "CET-1CEST,M3.5.0,M10.5.0/3";
  time_t epochs[] = {
      1704067200, 1711900800, 1720000000, 1730419200, 1735689600,
  };
  for (time_t epoch : epochs) {
    assert(verify_against_libc(tz_str, epoch));
  }
}

TEST(verify_angle_bracket_notation) {
  // This was the bug in espressif/newlib-esp32#8
  const char *tz_str = "<+07>-7";
  time_t epochs[] = {
      1704067200,
      1720000000,
      1735689600,
  };
  for (time_t epoch : epochs) {
    assert(verify_against_libc(tz_str, epoch));
  }
}

TEST(verify_india_half_hour) {
  const char *tz_str = "IST-5:30";
  time_t epochs[] = {
      1704067200,
      1720000000,
      1735689600,
  };
  for (time_t epoch : epochs) {
    assert(verify_against_libc(tz_str, epoch));
  }
}

TEST(verify_new_zealand_southern_hemisphere) {
  const char *tz_str = "NZST-12NZDT,M9.5.0,M4.1.0/3";
  time_t epochs[] = {
      1704067200,  // Jan (NZ summer, DST)
      1720000000,  // July (NZ winter, no DST)
      1735689600,  // Jan (NZ summer, DST)
  };
  for (time_t epoch : epochs) {
    assert(verify_against_libc(tz_str, epoch));
  }
}

TEST(verify_australia_sydney) {
  const char *tz_str = "AEST-10AEDT,M10.1.0,M4.1.0/3";
  time_t epochs[] = {
      1704067200,
      1720000000,
      1735689600,
  };
  for (time_t epoch : epochs) {
    assert(verify_against_libc(tz_str, epoch));
  }
}

// ============================================================================
// DST boundary edge cases
// ============================================================================

TEST(dst_boundary_just_before_spring_forward) {
  // Test 1 second before DST starts
  ParsedTimezone tz;
  parse_posix_tz("EST5EDT,M3.2.0/2,M11.1.0/2", tz);

  // March 8, 2026 06:59:59 UTC = 01:59:59 EST (1 second before spring forward)
  struct tm before {};
  before.tm_sec = 59;
  before.tm_min = 59;
  before.tm_hour = 6;
  before.tm_mday = 8;
  before.tm_mon = 2;
  before.tm_year = 126;
  time_t before_epoch = internal::tm_to_epoch_utc(&before);
  assert(is_in_dst(before_epoch, tz) == false);

  // March 8, 2026 07:00:00 UTC = 02:00:00 EST -> 03:00:00 EDT (DST started)
  struct tm after {};
  after.tm_sec = 0;
  after.tm_min = 0;
  after.tm_hour = 7;
  after.tm_mday = 8;
  after.tm_mon = 2;
  after.tm_year = 126;
  time_t after_epoch = internal::tm_to_epoch_utc(&after);
  assert(is_in_dst(after_epoch, tz) == true);
}

TEST(dst_boundary_just_before_fall_back) {
  // Test 1 second before DST ends
  ParsedTimezone tz;
  parse_posix_tz("EST5EDT,M3.2.0/2,M11.1.0/2", tz);

  // November 1, 2026 05:59:59 UTC = 01:59:59 EDT (1 second before fall back)
  struct tm before {};
  before.tm_sec = 59;
  before.tm_min = 59;
  before.tm_hour = 5;
  before.tm_mday = 1;
  before.tm_mon = 10;
  before.tm_year = 126;
  time_t before_epoch = internal::tm_to_epoch_utc(&before);
  assert(is_in_dst(before_epoch, tz) == true);

  // November 1, 2026 06:00:00 UTC = 02:00:00 EDT -> 01:00:00 EST (DST ended)
  struct tm after {};
  after.tm_sec = 0;
  after.tm_min = 0;
  after.tm_hour = 6;
  after.tm_mday = 1;
  after.tm_mon = 10;
  after.tm_year = 126;
  time_t after_epoch = internal::tm_to_epoch_utc(&after);
  assert(is_in_dst(after_epoch, tz) == false);
}

// ============================================================================
// Main
// ============================================================================

int main() {
  printf("POSIX TZ Parser Unit Tests\n");
  printf("==========================\n\n");

  printf("Basic TZ string parsing:\n");
  RUN_TEST(parse_simple_offset_est5);
  RUN_TEST(parse_negative_offset_cet);
  RUN_TEST(parse_explicit_positive_offset);
  RUN_TEST(parse_zero_offset);
  RUN_TEST(parse_us_eastern_with_dst);
  RUN_TEST(parse_us_central_with_time);
  RUN_TEST(parse_europe_berlin);
  RUN_TEST(parse_new_zealand);
  RUN_TEST(parse_explicit_dst_offset);

  printf("\nAngle-bracket notation (espressif/newlib-esp32#8):\n");
  RUN_TEST(parse_angle_bracket_positive);
  RUN_TEST(parse_angle_bracket_negative);
  RUN_TEST(parse_angle_bracket_with_dst);
  RUN_TEST(parse_angle_bracket_named);
  RUN_TEST(parse_angle_bracket_with_minutes);

  printf("\nHalf-hour and unusual offsets:\n");
  RUN_TEST(parse_offset_with_minutes_india);
  RUN_TEST(parse_offset_with_minutes_nepal);
  RUN_TEST(parse_offset_with_seconds);
  RUN_TEST(parse_chatham_islands);

  printf("\nInvalid input handling:\n");
  RUN_TEST(parse_empty_string_fails);
  RUN_TEST(parse_null_fails);
  RUN_TEST(parse_short_name_fails);
  RUN_TEST(parse_missing_offset_fails);
  RUN_TEST(parse_unterminated_bracket_fails);

  printf("\nJ-format and plain day number:\n");
  RUN_TEST(parse_j_format_basic);
  RUN_TEST(parse_j_format_with_time);
  RUN_TEST(parse_plain_day_number);
  RUN_TEST(parse_plain_day_number_with_time);
  RUN_TEST(j_format_invalid_day_zero);
  RUN_TEST(j_format_invalid_day_366);
  RUN_TEST(plain_day_invalid_day_366);

  printf("\nJulian day to month/day conversion:\n");
  RUN_TEST(julian_day_60_is_march_1);
  RUN_TEST(julian_day_1_is_jan_1);
  RUN_TEST(julian_day_365_is_dec_31);
  RUN_TEST(day_of_year_59_differs_by_leap);
  RUN_TEST(day_of_year_0_is_jan_1);

  printf("\nDay of week calculation:\n");
  RUN_TEST(day_of_week_known_dates);
  RUN_TEST(leap_year_detection);
  RUN_TEST(days_in_month_regular);
  RUN_TEST(days_in_month_leap_year);

  printf("\nDST transition calculation:\n");
  RUN_TEST(dst_start_us_eastern_2026);
  RUN_TEST(dst_end_us_eastern_2026);
  RUN_TEST(last_sunday_of_march_2026);
  RUN_TEST(last_sunday_of_october_2026);
  RUN_TEST(first_sunday_of_april_2026);

  printf("\nis_in_dst tests:\n");
  RUN_TEST(is_in_dst_us_eastern_summer);
  RUN_TEST(is_in_dst_us_eastern_winter);
  RUN_TEST(is_in_dst_no_dst_timezone);
  RUN_TEST(southern_hemisphere_dst_summer);
  RUN_TEST(southern_hemisphere_dst_winter);

  printf("\nepoch_to_local_tm tests:\n");
  RUN_TEST(epoch_to_local_basic);
  RUN_TEST(epoch_to_local_with_offset);
  RUN_TEST(epoch_to_local_dst_transition);

  printf("\nDST boundary edge cases:\n");
  RUN_TEST(dst_boundary_just_before_spring_forward);
  RUN_TEST(dst_boundary_just_before_fall_back);

  printf("\nVerification against libc:\n");
  RUN_TEST(verify_us_eastern_multiple_epochs);
  RUN_TEST(verify_us_central_multiple_epochs);
  RUN_TEST(verify_europe_berlin_multiple_epochs);
  RUN_TEST(verify_angle_bracket_notation);
  RUN_TEST(verify_india_half_hour);
  RUN_TEST(verify_new_zealand_southern_hemisphere);
  RUN_TEST(verify_australia_sydney);

  printf("\n==========================\n");
  printf("All tests passed!\n");

  return 0;
}
