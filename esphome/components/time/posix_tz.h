#pragma once

#ifdef USE_TIME_TIMEZONE

#include <cstdint>
#include <ctime>

namespace esphome::time {

/// Type of DST transition rule
enum class DSTRuleType : uint8_t {
  NONE = 0,        ///< No DST rule (used to indicate no DST)
  MONTH_WEEK_DAY,  ///< M format: Mm.w.d (e.g., M3.2.0 = 2nd Sunday of March)
  JULIAN_NO_LEAP,  ///< J format: Jn (day 1-365, Feb 29 not counted)
  DAY_OF_YEAR,     ///< Plain number: n (day 0-365, Feb 29 counted in leap years)
};

/// Rule for DST transition (packed for 32-bit: 12 bytes)
struct DSTRule {
  int32_t time_seconds;  ///< Seconds after midnight (default 7200 = 2:00 AM)
  uint16_t day;          ///< Day of year (for JULIAN_NO_LEAP and DAY_OF_YEAR)
  DSTRuleType type;      ///< Type of rule
  uint8_t month;         ///< Month 1-12 (for MONTH_WEEK_DAY)
  uint8_t week;          ///< Week 1-5, 5 = last (for MONTH_WEEK_DAY)
  uint8_t day_of_week;   ///< Day 0-6, 0 = Sunday (for MONTH_WEEK_DAY)
};

/// Parsed POSIX timezone information (packed for 32-bit: 32 bytes)
struct ParsedTimezone {
  int32_t std_offset_seconds;  ///< Standard time offset from UTC in seconds (positive = west)
  int32_t dst_offset_seconds;  ///< DST offset from UTC in seconds
  DSTRule dst_start;           ///< When DST starts
  DSTRule dst_end;             ///< When DST ends

  /// Check if this timezone has DST rules
  bool has_dst() const { return this->dst_start.type != DSTRuleType::NONE; }
};

/// Parse a POSIX TZ string into a ParsedTimezone struct.
///
/// @deprecated Remove before 2026.9.0 (bridge code for backward compatibility).
/// This parser only exists so that older Home Assistant clients that send the timezone
/// as a string (instead of the pre-parsed ParsedTimezone protobuf struct) can still
/// set the timezone on the device. Once all clients are updated to send the struct
/// directly, this function and all internal parsing helpers will be removed.
/// See https://github.com/esphome/backlog/issues/91
///
/// Supports formats like:
///   - "EST5" (simple offset, no DST)
///   - "EST5EDT,M3.2.0,M11.1.0" (with DST, M-format rules)
///   - "CST6CDT,M3.2.0/2,M11.1.0/2" (with transition times)
///   - "<+07>-7" (angle-bracket notation for special names)
///   - "IST-5:30" (half-hour offsets)
///   - "EST5EDT,J60,J300" (J-format: Julian day without leap day)
///   - "EST5EDT,60,300" (plain day number: day of year with leap day)
/// @param tz_string The POSIX TZ string to parse
/// @param result Output: the parsed timezone data
/// @return true if parsing succeeded, false on error
bool parse_posix_tz(const char *tz_string, ParsedTimezone &result);

/// Convert a UTC epoch to local time using the parsed timezone.
/// This replaces libc's localtime() to avoid scanf dependency.
/// @param utc_epoch Unix timestamp in UTC
/// @param tz The parsed timezone
/// @param[out] out_tm Output tm struct with local time
/// @return true on success
bool epoch_to_local_tm(time_t utc_epoch, const ParsedTimezone &tz, struct tm *out_tm);

/// Set the global timezone used by epoch_to_local_tm() when called without a timezone.
/// This is called by RealTimeClock::apply_timezone_() to enable ESPTime::from_epoch_local()
/// to work without libc's localtime().
void set_global_tz(const ParsedTimezone &tz);

/// Get the global timezone.
const ParsedTimezone &get_global_tz();

/// Check if a given UTC epoch falls within DST for the parsed timezone.
/// @param utc_epoch Unix timestamp in UTC
/// @param tz The parsed timezone
/// @return true if DST is in effect at the given time
bool is_in_dst(time_t utc_epoch, const ParsedTimezone &tz);

// Internal helper functions exposed for testing.
// Remove before 2026.9.0: skip_tz_name, parse_offset, parse_dst_rule are only
// used by parse_posix_tz() which is bridge code for backward compatibility.
// The remaining helpers (epoch_to_tm_utc, day_of_week, days_in_month, etc.)
// are used by the conversion functions and will stay.

namespace internal {

/// Skip a timezone name (letters or <...> quoted format)
/// @param p Pointer to current position, updated on return
/// @return true if a valid name was found
bool skip_tz_name(const char *&p);

/// Parse an offset in format [-]hh[:mm[:ss]]
/// @param p Pointer to current position, updated on return
/// @return Offset in seconds
int32_t parse_offset(const char *&p);

/// Parse a DST rule in format Mm.w.d[/time], Jn[/time], or n[/time]
/// @param p Pointer to current position, updated on return
/// @param rule Output: the parsed rule
/// @return true if parsing succeeded
bool parse_dst_rule(const char *&p, DSTRule &rule);

/// Convert Julian day (J format, 1-365 not counting Feb 29) to month/day
/// @param julian_day Day number 1-365
/// @param[out] month Output: month 1-12
/// @param[out] day Output: day of month
void julian_to_month_day(int julian_day, int &month, int &day);

/// Convert day of year (plain format, 0-365 counting Feb 29) to month/day
/// @param day_of_year Day number 0-365
/// @param year The year (for leap year calculation)
/// @param[out] month Output: month 1-12
/// @param[out] day Output: day of month
void day_of_year_to_month_day(int day_of_year, int year, int &month, int &day);

/// Calculate day of week for any date (0 = Sunday)
/// Uses a simplified algorithm that works for years 1970-2099
int day_of_week(int year, int month, int day);

/// Get the number of days in a month
int days_in_month(int year, int month);

/// Check if a year is a leap year
bool is_leap_year(int year);

/// Convert epoch to year/month/day/hour/min/sec (UTC)
void epoch_to_tm_utc(time_t epoch, struct tm *out_tm);

/// Calculate the epoch timestamp for a DST transition in a given year.
/// @param year The year (e.g., 2026)
/// @param rule The DST rule (month, week, day_of_week, time)
/// @param base_offset_seconds The timezone offset to apply (std or dst depending on context)
/// @return Unix epoch timestamp of the transition
time_t calculate_dst_transition(int year, const DSTRule &rule, int32_t base_offset_seconds);

}  // namespace internal

}  // namespace esphome::time

#endif  // USE_TIME_TIMEZONE
