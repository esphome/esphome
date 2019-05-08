#include "real_time_clock.h"
#include "esphome/core/log.h"
#include "lwip/opt.h"
#ifdef ARDUINO_ARCH_ESP8266
#include "sys/time.h"
#endif

namespace esphome {
namespace time {

static const char *TAG = "time";

RealTimeClock::RealTimeClock() = default;
void RealTimeClock::call_setup() {
  this->setup_internal_();
  setenv("TZ", this->timezone_.c_str(), 1);
  tzset();
  this->setup();
}
void RealTimeClock::synchronize_epoch_(uint32_t epoch) {
  struct timeval timev {
    .tv_sec = static_cast<time_t>(epoch), .tv_usec = 0,
  };
  timezone tz = {0, 0};
  settimeofday(&timev, &tz);

  auto time = this->now();
  char buf[128];
  time.strftime(buf, sizeof(buf), "%c");
  ESP_LOGD(TAG, "Synchronized time: %s", buf);
}

size_t ESPTime::strftime(char *buffer, size_t buffer_len, const char *format) {
  struct tm c_tm = this->to_c_tm();
  return ::strftime(buffer, buffer_len, format, &c_tm);
}
ESPTime ESPTime::from_c_tm(struct tm *c_tm, time_t c_time) {
  return ESPTime{.second = uint8_t(c_tm->tm_sec),
                 .minute = uint8_t(c_tm->tm_min),
                 .hour = uint8_t(c_tm->tm_hour),
                 .day_of_week = uint8_t(c_tm->tm_wday + 1),
                 .day_of_month = uint8_t(c_tm->tm_mday),
                 .day_of_year = uint16_t(c_tm->tm_yday + 1),
                 .month = uint8_t(c_tm->tm_mon + 1),
                 .year = uint16_t(c_tm->tm_year + 1900),
                 .is_dst = bool(c_tm->tm_isdst),
                 .time = c_time};
}
struct tm ESPTime::to_c_tm() {
  struct tm c_tm = tm{.tm_sec = this->second,
                      .tm_min = this->minute,
                      .tm_hour = this->hour,
                      .tm_mday = this->day_of_month,
                      .tm_mon = this->month - 1,
                      .tm_year = this->year - 1900,
                      .tm_wday = this->day_of_week - 1,
                      .tm_yday = this->day_of_year - 1,
                      .tm_isdst = this->is_dst};
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
bool ESPTime::is_valid() const { return this->year >= 2018; }

template<typename T> bool increment_time_value(T &current, uint16_t begin, uint16_t end) {
  current++;
  if (current >= end) {
    current = begin;
    return true;
  }
  return false;
}

void ESPTime::increment_second() {
  this->time++;
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

  static const uint8_t DAYS_IN_MONTH[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  uint8_t days_in_month = DAYS_IN_MONTH[this->month];
  if (this->month == 2 && this->year % 4 == 0)
    days_in_month = 29;

  if (increment_time_value(this->day_of_month, 1, days_in_month + 1)) {
    // day of month roll-over, increment month
    increment_time_value(this->month, 1, 13);
  }

  uint16_t days_in_year = (this->year % 4 == 0) ? 366 : 365;
  if (increment_time_value(this->day_of_year, 1, days_in_year + 1)) {
    // day of year roll-over, increment year
    this->year++;
  }
}
bool ESPTime::operator<(ESPTime other) { return this->time < other.time; }
bool ESPTime::operator<=(ESPTime other) { return this->time <= other.time; }
bool ESPTime::operator==(ESPTime other) { return this->time == other.time; }
bool ESPTime::operator>=(ESPTime other) { return this->time >= other.time; }
bool ESPTime::operator>(ESPTime other) { return this->time > other.time; }
bool ESPTime::in_range() const {
  return this->second < 61 && this->minute < 60 && this->hour < 24 && this->day_of_week > 0 && this->day_of_week < 8 &&
         this->day_of_month > 0 && this->day_of_month < 32 && this->day_of_year > 0 && this->day_of_year < 367 &&
         this->month > 0 && this->month < 13;
}

}  // namespace time
}  // namespace esphome
