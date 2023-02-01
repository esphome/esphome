#include "real_time_clock.h"
#include "esphome/core/log.h"
#include "lwip/opt.h"
#ifdef USE_ESP8266
#include "sys/time.h"
#endif
#ifdef USE_RP2040
#include <sys/time.h>
#endif
#include <cerrno>

namespace esphome {
namespace time {

static const char *const TAG = "time";

RealTimeClock::RealTimeClock() = default;
void RealTimeClock::call_setup() {
  this->apply_timezone_();
  PollingComponent::call_setup();
}
void RealTimeClock::synchronize_epoch_(uint32_t epoch) {
  // Update UTC epoch time.
  struct timeval timev {
    .tv_sec = static_cast<time_t>(epoch), .tv_usec = 0,
  };
  ESP_LOGVV(TAG, "Got epoch %u", epoch);
  timezone tz = {0, 0};
  int ret = settimeofday(&timev, &tz);
  if (ret == EINVAL) {
    // Some ESP8266 frameworks abort when timezone parameter is not NULL
    // while ESP32 expects it not to be NULL
    ret = settimeofday(&timev, nullptr);
  }

  // Move timezone back to local timezone.
  this->apply_timezone_();

  if (ret != 0) {
    ESP_LOGW(TAG, "setimeofday() failed with code %d", ret);
  }

  auto time = this->now();
  ESP_LOGD(TAG, "Synchronized time: %04d-%02d-%02d %02d:%02d:%02d", time.year, time.month, time.day_of_month, time.hour,
           time.minute, time.second);

  this->time_sync_callback_.call();
}

void RealTimeClock::apply_timezone_() {
  setenv("TZ", this->timezone_.c_str(), 1);
  tzset();
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
    timestr.resize(timestr.size() * 2);
    len = ::strftime(&timestr[0], timestr.size(), format.c_str(), &c_tm);
  }
  timestr.resize(len);
  return timestr;
}

template<typename T> bool increment_time_value(T &current, uint16_t begin, uint16_t end) {
  current++;
  if (current >= end) {
    current = begin;
    return true;
  }
  return false;
}

static bool is_leap_year(uint32_t year) { return (year % 4) == 0 && ((year % 100) != 0 || (year % 400) == 0); }

static uint8_t days_in_month(uint8_t month, uint16_t year) {
  static const uint8_t DAYS_IN_MONTH[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  uint8_t days = DAYS_IN_MONTH[month];
  if (month == 2 && is_leap_year(year))
    return 29;
  return days;
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
    res += is_leap_year(i) ? 366 : 365;

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

}  // namespace time
}  // namespace esphome
