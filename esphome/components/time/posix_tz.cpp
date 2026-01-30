#include "posix_tz.h"
#include <cctype>

namespace esphome {

namespace internal {

// Helper to parse an unsigned integer from string, updating pointer
static uint32_t parse_uint(const char *&p) {
  uint32_t value = 0;
  while (std::isdigit(static_cast<unsigned char>(*p))) {
    value = value * 10 + (*p - '0');
    p++;
  }
  return value;
}

bool is_leap_year(int year) { return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0); }

// Extract just the year from a UTC epoch (faster than full epoch_to_tm_utc)
static int epoch_to_year(time_t epoch) {
  int64_t days = epoch / 86400;
  if (epoch < 0 && epoch % 86400 != 0)
    days--;
  int year = 1970;
  while (days >= (is_leap_year(year) ? 366 : 365)) {
    days -= is_leap_year(year) ? 366 : 365;
    year++;
  }
  while (days < 0) {
    year--;
    days += is_leap_year(year) ? 366 : 365;
  }
  return year;
}

int days_in_month(int year, int month) {
  static const int DAYS_PER_MONTH[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month == 2 && is_leap_year(year))
    return 29;
  return DAYS_PER_MONTH[month - 1];
}

// Zeller-like algorithm for day of week (0 = Sunday)
int day_of_week(int year, int month, int day) {
  // Adjust for January/February
  if (month < 3) {
    month += 12;
    year--;
  }
  int k = year % 100;
  int j = year / 100;
  int h = (day + (13 * (month + 1)) / 5 + k + k / 4 + j / 4 - 2 * j) % 7;
  // Convert from Zeller (0=Sat) to standard (0=Sun)
  return ((h + 6) % 7);
}

void epoch_to_tm_utc(time_t epoch, struct tm *out_tm) {
  // Days since epoch
  int64_t days = epoch / 86400;
  int32_t remaining_secs = epoch % 86400;
  if (remaining_secs < 0) {
    days--;
    remaining_secs += 86400;
  }

  out_tm->tm_sec = remaining_secs % 60;
  remaining_secs /= 60;
  out_tm->tm_min = remaining_secs % 60;
  out_tm->tm_hour = remaining_secs / 60;

  // Day of week (Jan 1, 1970 was Thursday = 4)
  out_tm->tm_wday = static_cast<int>((days + 4) % 7);
  if (out_tm->tm_wday < 0)
    out_tm->tm_wday += 7;

  // Calculate year, month, day
  int year = 1970;
  while (days >= (is_leap_year(year) ? 366 : 365)) {
    days -= is_leap_year(year) ? 366 : 365;
    year++;
  }
  while (days < 0) {
    year--;
    days += is_leap_year(year) ? 366 : 365;
  }

  out_tm->tm_year = year - 1900;
  out_tm->tm_yday = static_cast<int>(days);

  int month = 1;
  while (days >= days_in_month(year, month)) {
    days -= days_in_month(year, month);
    month++;
  }

  out_tm->tm_mon = month - 1;
  out_tm->tm_mday = static_cast<int>(days) + 1;
  out_tm->tm_isdst = 0;
}

bool skip_tz_name(const char *&p) {
  if (*p == '<') {
    // Angle-bracket quoted name: <+07>, <-03>, <AEST>
    p++;  // skip '<'
    while (*p && *p != '>') {
      p++;
    }
    if (*p == '>') {
      p++;  // skip '>'
      return true;
    }
    return false;  // Unterminated
  }

  // Standard name: 3+ letters
  const char *start = p;
  while (*p && std::isalpha(static_cast<unsigned char>(*p))) {
    p++;
  }
  return (p - start) >= 3;
}

int32_t parse_offset(const char *&p) {
  int sign = 1;
  if (*p == '-') {
    sign = -1;
    p++;
  } else if (*p == '+') {
    p++;
  }

  int hours = parse_uint(p);
  int minutes = 0;
  int seconds = 0;

  if (*p == ':') {
    p++;
    minutes = parse_uint(p);
    if (*p == ':') {
      p++;
      seconds = parse_uint(p);
    }
  }

  return sign * (hours * 3600 + minutes * 60 + seconds);
}

// Helper to parse the optional /time suffix (reuses parse_offset logic)
static void parse_transition_time(const char *&p, DSTRule &rule) {
  rule.time_seconds = 2 * 3600;  // Default 02:00
  if (*p == '/') {
    p++;
    rule.time_seconds = parse_offset(p);
  }
}

void julian_to_month_day(int julian_day, int &out_month, int &out_day) {
  // J format: day 1-365, Feb 29 is NOT counted even in leap years
  // So day 60 is always March 1
  static const int DAYS_BEFORE_MONTH[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

  out_month = 1;
  for (int m = 11; m >= 0; m--) {
    if (julian_day > DAYS_BEFORE_MONTH[m]) {
      out_month = m + 1;
      out_day = julian_day - DAYS_BEFORE_MONTH[m];
      return;
    }
  }
  out_day = julian_day;
}

void day_of_year_to_month_day(int day_of_year, int year, int &out_month, int &out_day) {
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

bool parse_dst_rule(const char *&p, DSTRule &rule) {
  rule = {};  // Zero initialize

  if (*p == 'M' || *p == 'm') {
    // M format: Mm.w.d (month.week.day)
    rule.type = DSTRuleType::MONTH_WEEK_DAY;
    p++;

    rule.month = parse_uint(p);
    if (rule.month < 1 || rule.month > 12)
      return false;

    if (*p++ != '.')
      return false;

    rule.week = parse_uint(p);
    if (rule.week < 1 || rule.week > 5)
      return false;

    if (*p++ != '.')
      return false;

    rule.day_of_week = parse_uint(p);
    if (rule.day_of_week > 6)
      return false;

  } else if (*p == 'J' || *p == 'j') {
    // J format: Jn (Julian day 1-365, not counting Feb 29)
    rule.type = DSTRuleType::JULIAN_NO_LEAP;
    p++;

    rule.day = parse_uint(p);
    if (rule.day < 1 || rule.day > 365)
      return false;

  } else if (std::isdigit(static_cast<unsigned char>(*p))) {
    // Plain number format: n (day 0-365, counting Feb 29)
    rule.type = DSTRuleType::DAY_OF_YEAR;

    rule.day = parse_uint(p);
    if (rule.day > 365)
      return false;

  } else {
    return false;
  }

  // Parse optional /time suffix
  parse_transition_time(p, rule);

  return true;
}

}  // namespace internal

bool parse_posix_tz(const char *tz_string, ParsedTimezone &result) {
  if (!tz_string || !*tz_string) {
    return false;
  }

  const char *p = tz_string;

  // Initialize result
  result.std_offset_seconds = 0;
  result.dst_offset_seconds = 0;
  result.has_dst = false;
  result.dst_start = {};
  result.dst_end = {};

  // Skip standard timezone name
  if (!internal::skip_tz_name(p)) {
    return false;
  }

  // Parse standard offset (required)
  if (!*p || (!std::isdigit(static_cast<unsigned char>(*p)) && *p != '+' && *p != '-')) {
    return false;
  }
  result.std_offset_seconds = internal::parse_offset(p);

  // Check for DST name
  if (!*p) {
    return true;  // No DST
  }

  // If next char is comma, there's no DST name but there are rules (invalid)
  if (*p == ',') {
    return false;
  }

  if (!internal::skip_tz_name(p)) {
    return true;  // No valid DST name, no DST
  }

  // We have a DST name
  result.has_dst = true;

  // Optional DST offset (default is std - 1 hour)
  if (*p && *p != ',' && (std::isdigit(static_cast<unsigned char>(*p)) || *p == '+' || *p == '-')) {
    result.dst_offset_seconds = internal::parse_offset(p);
  } else {
    result.dst_offset_seconds = result.std_offset_seconds - 3600;
  }

  // Parse DST rules if present
  if (*p == ',') {
    p++;
    if (!internal::parse_dst_rule(p, result.dst_start)) {
      return false;
    }

    if (*p == ',') {
      p++;
      if (!internal::parse_dst_rule(p, result.dst_end)) {
        return false;
      }
    }
  }

  return true;
}

time_t calculate_dst_transition(int year, const DSTRule &rule, int32_t base_offset_seconds) {
  int month, day;

  switch (rule.type) {
    case DSTRuleType::MONTH_WEEK_DAY: {
      // Find the nth occurrence of day_of_week in the given month
      int first_day_of_month = internal::day_of_week(year, rule.month, 1);

      // Days until first occurrence of target day
      int days_until_first = (rule.day_of_week - first_day_of_month + 7) % 7;
      int first_occurrence = 1 + days_until_first;

      if (rule.week == 5) {
        // "Last" occurrence - find the last one in the month
        int days_in_m = internal::days_in_month(year, rule.month);
        day = first_occurrence;
        while (day + 7 <= days_in_m) {
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
      internal::julian_to_month_day(rule.day, month, day);
      break;

    case DSTRuleType::DAY_OF_YEAR:
      // Plain format: day 0-365, Feb 29 counted
      internal::day_of_year_to_month_day(rule.day, year, month, day);
      break;
  }

  // Calculate days from epoch to this date
  int64_t days = 0;
  for (int y = 1970; y < year; y++) {
    days += internal::is_leap_year(y) ? 366 : 365;
  }
  for (int m = 1; m < month; m++) {
    days += internal::days_in_month(year, m);
  }
  days += day - 1;

  // Convert to epoch and add transition time and base offset
  return days * 86400 + rule.time_seconds + base_offset_seconds;
}

bool is_in_dst(time_t utc_epoch, const ParsedTimezone &tz) {
  if (!tz.has_dst) {
    return false;
  }

  int year = internal::epoch_to_year(utc_epoch);

  // Calculate DST start and end for this year
  // DST start transition happens in standard time
  time_t dst_start = calculate_dst_transition(year, tz.dst_start, tz.std_offset_seconds);
  // DST end transition happens in daylight time
  time_t dst_end = calculate_dst_transition(year, tz.dst_end, tz.dst_offset_seconds);

  if (dst_start < dst_end) {
    // Northern hemisphere: DST is between start and end
    return (utc_epoch >= dst_start && utc_epoch < dst_end);
  } else {
    // Southern hemisphere: DST is outside the range (wraps around year)
    return (utc_epoch >= dst_start || utc_epoch < dst_end);
  }
}

bool epoch_to_local_tm(time_t utc_epoch, const ParsedTimezone &tz, struct tm *out_tm) {
  if (!out_tm) {
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

}  // namespace esphome
