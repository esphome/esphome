#include "esphome/core/defines.h"

#ifdef USE_TIME_TIMEZONE

#include "posix_tz.h"
#include <algorithm>
#include <cstdio>

namespace esphome::time {

// Global timezone - set once at startup, rarely changes
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables) - intentional mutable state
static ParsedTimezone global_tz_{};

void set_global_tz(const ParsedTimezone &tz) { global_tz_ = tz; }

const ParsedTimezone &get_global_tz() { return global_tz_; }

namespace internal {

time_t __attribute__((noinline)) calculate_dst_transition(int year, const DSTRule &rule, int32_t base_offset_seconds) {
  const int32_t year_start = days_to_year_start(year);
  int day_offset = 0;  // days after Jan 1

  switch (rule.type) {
    case DSTRuleType::MONTH_WEEK_DAY: {
      // Find the nth occurrence of day_of_week in the given month
      const int month_offset = days_before_month(year, rule.month);
      int first_dow = days_to_weekday(year_start + month_offset);

      // Days until first occurrence of target day
      int days_until_first = (rule.day_of_week - first_dow + 7) % 7;
      int day = 1 + days_until_first;

      if (rule.week == 5) {
        // "Last" occurrence - find the last one in the month
        int dim = days_in_month(year, rule.month);
        while (day + 7 <= dim) {
          day += 7;
        }
      } else {
        // nth occurrence
        day += (rule.week - 1) * 7;
      }
      day_offset = month_offset + day - 1;
      break;
    }

    case DSTRuleType::JULIAN_NO_LEAP:
      // J format: day 1-365, Feb 29 not counted, so J60 is always March 1
      day_offset = rule.day - 1 + (rule.day >= 60 && is_leap_year(year));
      break;

    case DSTRuleType::DAY_OF_YEAR:
      // Plain format: day 0-365, Feb 29 counted; a day past the end of the year is Dec 31
      day_offset = std::min<int>(rule.day, is_leap_year(year) ? 365 : 364);
      break;

    case DSTRuleType::NONE:
      break;
  }

  // Convert to epoch and add transition time and base offset
  int64_t days = year_start + day_offset;
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
  if (timer == nullptr) {
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
