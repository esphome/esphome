#include "esphome/core/defines.h"

#ifdef USE_TIME_TIMEZONE

#include "posix_tz.h"
#include <cstdio>

namespace esphome::time {

// Global timezone - set once at startup, rarely changes
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables) - intentional mutable state
static ParsedTimezone global_tz_{};

void set_global_tz(const ParsedTimezone &tz) { global_tz_ = tz; }

const ParsedTimezone &get_global_tz() { return global_tz_; }

namespace internal {

static constexpr int SECONDS_PER_DAY = 86400;

void __attribute__((noinline)) julian_to_month_day(int julian_day, int &out_month, int &out_day) {
  // J format: day 1-365, Feb 29 is NOT counted even in leap years
  // So day 60 is always March 1
  // Iterate forward through months (no array needed)
  int remaining = julian_day;
  out_month = 1;
  while (out_month <= 12) {
    // Days in month for non-leap year (J format ignores leap years)
    int dim = days_in_month(2001, out_month);  // 2001 is non-leap year
    if (remaining <= dim) {
      out_day = remaining;
      return;
    }
    remaining -= dim;
    out_month++;
  }
  out_day = remaining;
}

void __attribute__((noinline)) day_of_year_to_month_day(int day_of_year, int year, int &out_month, int &out_day) {
  // Plain format: day 0-365, Feb 29 IS counted in leap years
  // Day 0 = Jan 1
  int remaining = day_of_year;
  out_month = 1;

  while (out_month <= 12) {
    int days_this_month = days_in_month(year, out_month);
    if (remaining < days_this_month) {
      out_day = remaining + 1;
      return;
    }
    remaining -= days_this_month;
    out_month++;
  }

  // Shouldn't reach here with valid input
  out_month = 12;
  out_day = 31;
}

// Calculate days from Jan 1 of given year to given month/day
static int __attribute__((noinline)) days_from_year_start(int year, int month, int day) {
  int days = day - 1;
  for (int m = 1; m < month; m++) {
    days += days_in_month(year, m);
  }
  return days;
}

// Day of week (0 = Sunday); Jan 1, 1970 was a Thursday
int __attribute__((noinline)) day_of_week(int year, int month, int day) {
  int wday = (days_to_year_start(year) + days_from_year_start(year, month, day) + 4) % 7;
  return wday < 0 ? wday + 7 : wday;
}

time_t __attribute__((noinline)) calculate_dst_transition(int year, const DSTRule &rule, int32_t base_offset_seconds) {
  int month = 1;
  int day = 1;

  switch (rule.type) {
    case DSTRuleType::MONTH_WEEK_DAY: {
      // Find the nth occurrence of day_of_week in the given month
      int first_dow = day_of_week(year, rule.month, 1);

      // Days until first occurrence of target day
      int days_until_first = (rule.day_of_week - first_dow + 7) % 7;
      int first_occurrence = 1 + days_until_first;

      if (rule.week == 5) {
        // "Last" occurrence - find the last one in the month
        int dim = days_in_month(year, rule.month);
        day = first_occurrence;
        while (day + 7 <= dim) {
          day += 7;
        }
      } else {
        // nth occurrence
        day = first_occurrence + (rule.week - 1) * 7;
      }
      month = rule.month;
      break;
    }

    case DSTRuleType::JULIAN_NO_LEAP:
      // J format: day 1-365, Feb 29 not counted
      julian_to_month_day(rule.day, month, day);
      break;

    case DSTRuleType::DAY_OF_YEAR:
      // Plain format: day 0-365, Feb 29 counted
      day_of_year_to_month_day(rule.day, year, month, day);
      break;

    case DSTRuleType::NONE:
      // Should never be called with NONE, but handle it gracefully
      month = 1;
      day = 1;
      break;
  }

  // Calculate days from epoch to this date
  int64_t days = days_to_year_start(year) + days_from_year_start(year, month, day);

  // Convert to epoch and add transition time and base offset
  return days * SECONDS_PER_DAY + rule.time_seconds + base_offset_seconds;
}

}  // namespace internal

bool __attribute__((noinline)) is_in_dst(time_t utc_epoch, const ParsedTimezone &tz) {
  if (!tz.has_dst()) {
    return false;
  }

  int year = internal::epoch_to_year(utc_epoch);

  // Calculate DST start and end for this year
  // DST start transition happens in standard time
  time_t dst_start = internal::calculate_dst_transition(year, tz.dst_start, tz.std_offset_seconds);
  // DST end transition happens in daylight time
  time_t dst_end = internal::calculate_dst_transition(year, tz.dst_end, tz.dst_offset_seconds);

  if (dst_start < dst_end) {
    // Northern hemisphere: DST is between start and end
    return (utc_epoch >= dst_start && utc_epoch < dst_end);
  } else {
    // Southern hemisphere: DST is outside the range (wraps around year)
    return (utc_epoch >= dst_start || utc_epoch < dst_end);
  }
}

// Format a POSIX offset (positive = west) as "+HHMM" / "-HHMM" for display.
// Convention: negate POSIX sign so east-of-UTC is positive (ISO 8601 / RFC 2822).
void format_designation(int32_t posix_offset, char *buf, size_t buf_size) {
  int32_t display = -posix_offset;
  char sign = display >= 0 ? '+' : '-';
  if (display < 0)
    display = -display;
  int h = display / 3600;
  int m = (display % 3600) / 60;
  snprintf(buf, buf_size, "%c%02d%02d", sign, h, m);
}

bool epoch_to_local_tm(time_t utc_epoch, const ParsedTimezone &tz, struct tm *out_tm) {
  // A far-off epoch would also stall the year search; a local offset keeps an in-range one at year -1 or later
  if (!out_tm || !internal::epoch_in_range(utc_epoch)) {
    return false;
  }

  // Determine DST status once (avoids duplicate is_in_dst calculation)
  bool in_dst = is_in_dst(utc_epoch, tz);
  int32_t offset = in_dst ? tz.dst_offset_seconds : tz.std_offset_seconds;

  // Apply offset (POSIX offset is positive west, so subtract to get local)
  time_t local_epoch = utc_epoch - offset;

  internal::epoch_to_tm_utc(local_epoch, out_tm);
  out_tm->tm_isdst = in_dst ? 1 : 0;

  return true;
}

}  // namespace esphome::time

#ifndef USE_HOST
// Override libc's localtime functions to use our timezone on embedded platforms.
// This allows user lambdas calling ::localtime() to get correct local time
// without needing the TZ environment variable (which pulls in scanf bloat).
// On host, we use the normal TZ mechanism since there's no memory constraint.

// Thread-safe version
extern "C" struct tm *localtime_r(const time_t *timer, struct tm *result) {
  if (timer == nullptr || result == nullptr) {
    return nullptr;
  }
  return esphome::time::epoch_to_local_tm(*timer, esphome::time::get_global_tz(), result) ? result : nullptr;
}

// Non-thread-safe version (uses static buffer, standard libc behavior)
extern "C" struct tm *localtime(const time_t *timer) {
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
  static struct tm localtime_buf;
  return localtime_r(timer, &localtime_buf);
}
#endif  // !USE_HOST

#endif  // USE_TIME_TIMEZONE
