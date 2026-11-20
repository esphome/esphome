#pragma once

#include <cstdint>
#include <ctime>

// UTC calendar math for ESPTime and the time component's timezone code, so neither needs libc's gmtime()
namespace esphome::internal {

/// Check if a year is a leap year
bool is_leap_year(int year);

/// Get the number of days in a month (1-12)
int days_in_month(int year, int month);

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
