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

/// Convert days since the epoch to a year, leaving the day of that year in days
int days_to_year(int32_t &days);

/// Whole days since the epoch, rounded down; secs gets the seconds into that day.
/// The epoch must be within the years 0 to 65535, like the functions below.
int32_t epoch_to_days(time_t epoch, int32_t &secs);

/// Convert epoch to year/month/day/hour/min/sec (UTC)
void epoch_to_tm_utc(time_t epoch, struct tm *out_tm);

}  // namespace esphome::internal
