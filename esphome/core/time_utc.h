#pragma once

#include <cstdint>
#include <ctime>

// UTC calendar math for ESPTime and the time component's timezone code, so neither needs libc's gmtime()
namespace esphome::internal {

static constexpr int32_t SECONDS_PER_DAY = 86400;

/// Clocks before this year (and epoch) are taken as not set yet
static constexpr int MIN_VALID_YEAR = 2019;
static constexpr time_t MIN_VALID_EPOCH = 1546300800;  // 2019-01-01 00:00:00 UTC

/// Check if a year is a leap year
bool is_leap_year(int year);

/// Get the number of days in a month (1-12)
int days_in_month(int year, int month);

/// Days in the year before the first of a month (1-12)
inline int days_before_month(int year, int month) {
  int days = 0;
  for (int m = 1; m < month; m++)
    days += days_in_month(year, m);
  return days;
}

/// Day of the week (0 = Sunday) of a day counted from the epoch, which may be negative
constexpr int days_to_weekday(int32_t days) {
  const int wday = (days + 4) % 7;  // Jan 1, 1970 was a Thursday
  return wday < 0 ? wday + 7 : wday;
}

/// Days from the epoch (Jan 1 1970) to Jan 1 of the given year
int32_t days_to_year_start(int year);

/// Whether a UTC epoch is in the years 0 to 65535, which the epoch functions below take without checking.
/// A 32-bit time_t never leaves that range.
inline bool epoch_in_range(time_t epoch) {
  if constexpr (sizeof(time_t) > sizeof(int32_t)) {
    return epoch >= -62167219200LL && epoch < 2005949145600LL;
  }
  return true;
}

/// Get the year of a UTC epoch
int epoch_to_year(time_t epoch);

/// Convert epoch to year/month/day/hour/min/sec (UTC)
void epoch_to_tm_utc(time_t epoch, struct tm *out_tm);

}  // namespace esphome::internal
