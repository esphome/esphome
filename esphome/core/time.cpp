#include "time.h"  // NOLINT
#include "helpers.h"

#include <cinttypes>

namespace esphome {

uint8_t days_in_month(uint8_t month, uint16_t year) {
  static const uint8_t DAYS_IN_MONTH[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month == 2 && (year % 4 == 0))
    return 29;
  return DAYS_IN_MONTH[month];
}

size_t ESPTime::strftime(char *buffer, size_t buffer_len, const char *format) {
  struct tm c_tm = this->to_c_tm();
  return ::strftime(buffer, buffer_len, format, &c_tm);
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

std::string ESPTime::strftime(const std::string &format) {
  std::string timestr;
  timestr.resize(format.size() * 4);
  struct tm c_tm = this->to_c_tm();
  size_t len = ::strftime(&timestr[0], timestr.size(), format.c_str(), &c_tm);
  while (len == 0) {
    if (timestr.size() >= 128) {
      // strftime has failed for reasons unrelated to the size of the buffer
      // so return a formatting error
      return "ERROR";
    }
    timestr.resize(timestr.size() * 2);
    len = ::strftime(&timestr[0], timestr.size(), format.c_str(), &c_tm);
  }
  timestr.resize(len);
  return timestr;
}

bool ESPTime::strptime(const std::string &time_to_parse, ESPTime &esp_time) {
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  int num;

  if (sscanf(time_to_parse.c_str(), "%04hu-%02hhu-%02hhu %02hhu:%02hhu:%02hhu %n", &year, &month, &day,  // NOLINT
             &hour,                                                                                      // NOLINT
             &minute,                                                                                    // NOLINT
             &second, &num) == 6 &&                                                                      // NOLINT
      num == time_to_parse.size()) {
    esp_time.year = year;
    esp_time.month = month;
    esp_time.day_of_month = day;
    esp_time.hour = hour;
    esp_time.minute = minute;
    esp_time.second = second;
  } else if (sscanf(time_to_parse.c_str(), "%04hu-%02hhu-%02hhu %02hhu:%02hhu %n", &year, &month, &day,  // NOLINT
                    &hour,                                                                               // NOLINT
                    &minute, &num) == 5 &&                                                               // NOLINT
             num == time_to_parse.size()) {
    esp_time.year = year;
    esp_time.month = month;
    esp_time.day_of_month = day;
    esp_time.hour = hour;
    esp_time.minute = minute;
    esp_time.second = 0;
  } else if (sscanf(time_to_parse.c_str(), "%02hhu:%02hhu:%02hhu %n", &hour, &minute, &second, &num) == 3 &&  // NOLINT
             num == time_to_parse.size()) {
    esp_time.hour = hour;
    esp_time.minute = minute;
    esp_time.second = second;
  } else if (sscanf(time_to_parse.c_str(), "%02hhu:%02hhu %n", &hour, &minute, &num) == 2 &&  // NOLINT
             num == time_to_parse.size()) {
    esp_time.hour = hour;
    esp_time.minute = minute;
    esp_time.second = 0;
  } else if (sscanf(time_to_parse.c_str(), "%04hu-%02hhu-%02hhu %n", &year, &month, &day, &num) == 3 &&  // NOLINT
             num == time_to_parse.size()) {
    esp_time.year = year;
    esp_time.month = month;
    esp_time.day_of_month = day;
  } else {
    return false;
  }
  return true;
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
  if (!this->fields_in_range()) {
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
  struct tm tm;

  tm.tm_year = this->year - 1900;
  tm.tm_mon = this->month - 1;
  tm.tm_mday = this->day_of_month;
  tm.tm_hour = this->hour;
  tm.tm_min = this->minute;
  tm.tm_sec = this->second;
  tm.tm_isdst = -1;

  this->timestamp = mktime(&tm);
}

int32_t ESPTime::timezone_offset() {
  int32_t offset = 0;
  time_t now = ::time(nullptr);
  auto local = ESPTime::from_epoch_local(now);
  auto utc = ESPTime::from_epoch_utc(now);
  bool negative = utc.hour > local.hour && local.day_of_year <= utc.day_of_year;

  if (utc.minute > local.minute) {
    local.minute += 60;
    local.hour -= 1;
  }
  offset += (local.minute - utc.minute) * 60;

  if (negative) {
    offset -= (utc.hour - local.hour) * 3600;
  } else {
    if (utc.hour > local.hour) {
      local.hour += 24;
    }
    offset += (local.hour - utc.hour) * 3600;
  }
  return offset;
}

bool ESPTime::operator<(ESPTime other) { return this->timestamp < other.timestamp; }
bool ESPTime::operator<=(ESPTime other) { return this->timestamp <= other.timestamp; }
bool ESPTime::operator==(ESPTime other) { return this->timestamp == other.timestamp; }
bool ESPTime::operator>=(ESPTime other) { return this->timestamp >= other.timestamp; }
bool ESPTime::operator>(ESPTime other) { return this->timestamp > other.timestamp; }

template<typename T> bool increment_time_value(T &current, uint16_t begin, uint16_t end) {
  current++;
  if (current >= end) {
    current = begin;
    return true;
  }
  return false;
}

}  // namespace esphome
