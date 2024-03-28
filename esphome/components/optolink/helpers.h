#pragma once

#ifdef USE_ARDUINO

#include <cstdint>
#include <string>

namespace esphome {
namespace optolink {

struct Time {
  int hours;
  int minutes;
};

inline bool check_time_sequence(const Time &t1, const Time &t2) {
  return t2.hours > t1.hours || (t2.hours == t1.hours && t2.minutes >= t1.minutes);
}

inline bool check_time_values(const Time &time) {
  return (time.hours >= 0 && time.hours <= 23) && (time.minutes >= 0 && time.minutes <= 59);
}

void rtrim(std::string &s);

std::string decode_day_schedule(const uint8_t *input);

uint8_t *encode_day_schedule(const std::string &input, uint8_t *output);

}  // namespace optolink
}  // namespace esphome

#endif
