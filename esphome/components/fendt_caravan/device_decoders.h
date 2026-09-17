#pragma once

#ifdef USE_ESP32
#include "esphome/core/log.h"
#include "esphome/core/string_ref.h"
#include <algorithm>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <string>
#include <vector>
#include <charconv>
#include <optional>

namespace esphome::fendt_caravan {

class DeviceDecoders {
 public:
  static bool decode_bool(const std::string &value) {
    std::string decode = value;
    std::transform(decode.begin(), decode.end(), decode.begin(), [](unsigned char c) { return std::tolower(c); });
    return (decode == "01") || (decode == "true") || (decode == "1") || (decode == "on");
  }
  static std::string decode_str(const std::string &value) { return value; }
  static std::string decode_bool_str(const std::string &value, const char **text) {
    return decode_bool(value) ? std::string(text[0]) : std::string(text[1]);
  }
  static float decode_temperature(const std::string &data);
  static float decode_voltage(const std::string &data);
  static int decode_int(const std::string &data);
  static time_t decode_date(const std::string &data);
  static time_t decode_time(const std::string &data);

  static std::string decode_heater_el(const std::string &data) {
    std::string ret_val = data;
    return ret_val;
  }
  static std::string decode_int_str(const std::string &data, const std::vector<std::string> &list);

 private:
  template<typename T> static std::optional<T> parse_data(const std::string &str);
};

}  // namespace esphome::fendt_caravan
#endif
