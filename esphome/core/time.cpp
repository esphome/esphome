#include "time.h"  // NOLINT
#include "helpers.h"

#include <algorithm>

namespace esphome {

uint8_t days_in_month(uint8_t month, uint16_t year) {
  static const uint8_t DAYS_IN_MONTH[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month == 2 && (year % 4 == 0) && (year % 100 != 0 || year % 400 == 0))
    return 29;
  return DAYS_IN_MONTH[month];
}

size_t ESPTime::strftime(char *buffer, size_t buffer_len, const char *format) {
  struct tm c_tm = this->to_c_tm();
  return ::strftime(buffer, buffer_len, format, &c_tm);
}

size_t ESPTime::strftime_to(std::span<char, STRFTIME_BUFFER_SIZE> buffer, const char *format) {
  struct tm c_tm = this->to_c_tm();
  size_t len = ::strftime(buffer.data(), buffer.size(), format, &c_tm);
  if (len > 0) {
    return len;
  }
  // Write "ERROR" to buffer on failure for consistent behavior
  constexpr char error_str[] = "ERROR";
  std::copy_n(error_str, sizeof(error_str), buffer.data());
  return sizeof(error_str) - 1;  // Length excluding null terminator
}

ESPTime ESPTime::from_c_tm(struct tm *c_tm, time_t c_time) {
  ESPTime res{};
  res.second = uint8_t(c_tm->tm_sec);
  res.minute = uint8_t(c_tm->tm_min);
  res.hour = uint8_t(c_tm->tm_hour);
  res.day_of_week = uint8_t(c_tm->tm_wday + 1);
  res.day_of_month = uint8_t(c_tm->tm_mday);
  res.day_of_year = uint16_t(c_tm->tm_yday + 1);
  res.month = uint8_t(c_tm->tm_mon + 1);
  res.year = uint16_t(c_tm->tm_year + 1900);
  res.is_dst = bool(c_tm->tm_isdst);
  res.timestamp = c_time;
  return res;
}

struct tm ESPTime::to_c_tm() {
  struct tm c_tm {};
  c_tm.tm_sec = this->second;
  c_tm.tm_min = this->minute;
  c_tm.tm_hour = this->hour;
  c_tm.tm_mday = this->day_of_month;
  c_tm.tm_mon = this->month - 1;
  c_tm.tm_year = this->year - 1900;
  c_tm.tm_wday = this->day_of_week - 1;
  c_tm.tm_yday = this->day_of_year - 1;
  c_tm.tm_isdst = this->is_dst;
  return c_tm;
}

std::string ESPTime::strftime(const char *format) {
  char buf[STRFTIME_BUFFER_SIZE];
  size_t len = this->strftime_to(buf, format);
  return std::string(buf, len);
}

std::string ESPTime::strftime(const std::string &format) { return this->strftime(format.c_str()); }

// Helper to parse exactly N digits, returns false if not enough digits
static bool parse_digits(const char *&p, const char *end, int count, uint16_t &value) {
  value = 0;
  for (int i = 0; i < count; i++) {
    if (p >= end || *p < '0' || *p > '9')
      return false;
    value = value * 10 + (*p - '0');
    p++;
  }
  return true;
}

// Helper to check for expected character
static bool expect_char(const char *&p, const char *end, char expected) {
  if (p >= end || *p != expected)
    return false;
  p++;
  return true;
}

bool ESPTime::strptime(const char *time_to_parse, size_t len, ESPTime &esp_time) {
  // Supported formats:
  //   YYYY-MM-DD HH:MM:SS (19 chars)
  //   YYYY-MM-DD HH:MM    (16 chars)
  //   YYYY-MM-DD          (10 chars)
  //   HH:MM:SS            (8 chars)
  //   HH:MM               (5 chars)

  if (time_to_parse == nullptr || len == 0)
    return false;

  const char *p = time_to_parse;
  const char *end = time_to_parse + len;
  uint16_t v1, v2, v3, v4, v5, v6;

  // Try date formats first (start with 4-digit year)
  if (len >= 10 && time_to_parse[4] == '-') {
    // YYYY-MM-DD...
    if (!parse_digits(p, end, 4, v1))
      return false;
    if (!expect_char(p, end, '-'))
      return false;
    if (!parse_digits(p, end, 2, v2))
      return false;
    if (!expect_char(p, end, '-'))
      return false;
    if (!parse_digits(p, end, 2, v3))
      return false;

    esp_time.year = v1;
    esp_time.month = v2;
    esp_time.day_of_month = v3;

    if (p == end) {
      // YYYY-MM-DD (date only)
      return true;
    }

    if (!expect_char(p, end, ' '))
      return false;

    // Continue with time part: HH:MM[:SS]
    if (!parse_digits(p, end, 2, v4))
      return false;
    if (!expect_char(p, end, ':'))
      return false;
    if (!parse_digits(p, end, 2, v5))
      return false;

    esp_time.hour = v4;
    esp_time.minute = v5;

    if (p == end) {
      // YYYY-MM-DD HH:MM
      esp_time.second = 0;
      return true;
    }

    if (!expect_char(p, end, ':'))
      return false;
    if (!parse_digits(p, end, 2, v6))
      return false;

    esp_time.second = v6;
    return p == end;  // YYYY-MM-DD HH:MM:SS
  }

  // Try time-only formats (HH:MM[:SS])
  if (len >= 5 && time_to_parse[2] == ':') {
    if (!parse_digits(p, end, 2, v1))
      return false;
    if (!expect_char(p, end, ':'))
      return false;
    if (!parse_digits(p, end, 2, v2))
      return false;

    esp_time.hour = v1;
    esp_time.minute = v2;

    if (p == end) {
      // HH:MM
      esp_time.second = 0;
      return true;
    }

    if (!expect_char(p, end, ':'))
      return false;
    if (!parse_digits(p, end, 2, v3))
      return false;

    esp_time.second = v3;
    return p == end;  // HH:MM:SS
  }

  return false;
}

void ESPTime::increment_second() {
  this->timestamp++;
  if (!increment_time_value(this->second, 0, 60))
    return;

  // second roll-over, increment minute
  if (!increment_time_value(this->minute, 0, 60))
    return;

  // minute roll-over, increment hour
  if (!increment_time_value(this->hour, 0, 24))
    return;

  // hour roll-over, increment day
  increment_time_value(this->day_of_week, 1, 8);

  if (increment_time_value(this->day_of_month, 1, days_in_month(this->month, this->year) + 1)) {
    // day of month roll-over, increment month
    increment_time_value(this->month, 1, 13);
  }

  uint16_t days_in_year = (this->year % 4 == 0) ? 366 : 365;
  if (increment_time_value(this->day_of_year, 1, days_in_year + 1)) {
    // day of year roll-over, increment year
    this->year++;
  }
}

void ESPTime::increment_day() {
  this->timestamp += 86400;

  // increment day
  increment_time_value(this->day_of_week, 1, 8);

  if (increment_time_value(this->day_of_month, 1, days_in_month(this->month, this->year) + 1)) {
    // day of month roll-over, increment month
    increment_time_value(this->month, 1, 13);
  }

  uint16_t days_in_year = (this->year % 4 == 0) ? 366 : 365;
  if (increment_time_value(this->day_of_year, 1, days_in_year + 1)) {
    // day of year roll-over, increment year
    this->year++;
  }
}

void ESPTime::recalc_timestamp_utc(bool use_day_of_year) {
  time_t res = 0;
  if (!this->fields_in_range(false, use_day_of_year)) {
    this->timestamp = -1;
    return;
  }

  for (int i = 1970; i < this->year; i++)
    res += (i % 4 == 0) ? 366 : 365;

  if (use_day_of_year) {
    res += this->day_of_year - 1;
  } else {
    for (int i = 1; i < this->month; i++)
      res += days_in_month(i, this->year);
    res += this->day_of_month - 1;
  }

  res *= 24;
  res += this->hour;
  res *= 60;
  res += this->minute;
  res *= 60;
  res += this->second;
  this->timestamp = res;
}

void ESPTime::recalc_timestamp_local() {
#ifdef USE_TIME_TIMEZONE
  // Calculate timestamp as if fields were UTC
  this->recalc_timestamp_utc(false);
  if (this->timestamp == -1) {
    return;  // Invalid time
  }

  // Now convert from local to UTC by adding the offset
  // POSIX: local = utc - offset, so utc = local + offset
  const auto &tz = time::get_global_tz();

  if (!tz.has_dst()) {
    // No DST - just apply standard offset
    this->timestamp += tz.std_offset_seconds;
    return;
  }

  // Try both interpretations to match libc mktime() with tm_isdst=-1
  // For ambiguous times (fall-back repeated hour), prefer standard time
  // For invalid times (spring-forward skipped hour), libc normalizes forward
  time_t utc_if_dst = this->timestamp + tz.dst_offset_seconds;
  time_t utc_if_std = this->timestamp + tz.std_offset_seconds;

  bool dst_valid = time::is_in_dst(utc_if_dst, tz);
  bool std_valid = !time::is_in_dst(utc_if_std, tz);

  if (dst_valid && !std_valid) {
    // Only DST interpretation is valid
    this->timestamp = utc_if_dst;
  } else {
    // All other cases use standard offset:
    // - Both valid (ambiguous fall-back repeated hour): prefer standard time
    // - Only standard valid: straightforward
    // - Neither valid (spring-forward skipped hour): std offset normalizes
    //   forward to match libc mktime(), e.g. 02:30 CST -> 03:30 CDT
    this->timestamp = utc_if_std;
  }
#else
  // No timezone support - treat as UTC
  this->recalc_timestamp_utc(false);
#endif
}

int32_t ESPTime::timezone_offset() {
#ifdef USE_TIME_TIMEZONE
  time_t now = ::time(nullptr);
  const auto &tz = time::get_global_tz();
  // POSIX offset is positive west, but we return offset to add to UTC to get local
  // So we negate the POSIX offset
  if (time::is_in_dst(now, tz)) {
    return -tz.dst_offset_seconds;
  }
  return -tz.std_offset_seconds;
#else
  // No timezone support - no offset
  return 0;
#endif
}

bool ESPTime::operator<(const ESPTime &other) const { return this->timestamp < other.timestamp; }
bool ESPTime::operator<=(const ESPTime &other) const { return this->timestamp <= other.timestamp; }
bool ESPTime::operator==(const ESPTime &other) const { return this->timestamp == other.timestamp; }
bool ESPTime::operator>=(const ESPTime &other) const { return this->timestamp >= other.timestamp; }
bool ESPTime::operator>(const ESPTime &other) const { return this->timestamp > other.timestamp; }

template<typename T> bool increment_time_value(T &current, uint16_t begin, uint16_t end) {
  current++;
  if (current >= end) {
    current = begin;
    return true;
  }
  return false;
}

}  // namespace esphome
