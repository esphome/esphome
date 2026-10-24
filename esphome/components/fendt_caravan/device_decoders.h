#pragma once

#ifdef USE_ESP32
#include "esphome/core/log.h"
#include "esphome/core/string_ref.h"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <optional>
#include <string>
#include <vector>

namespace esphome::fendt_caravan {

class DeviceDecoders {
 public:
  static bool decode_bool(const std::string &value) {
    std::string decoded = value;
    std::transform(decoded.begin(), decoded.end(), decoded.begin(),
                   [](unsigned char ch) { return std::tolower(ch); });
    return decoded == "01" || decoded == "true" || decoded == "1" || decoded == "on";
  }
  static std::string decode_str(const std::string &value) { return value; }
  static std::string decode_bool_str(const std::string &value, const char *const *text) {
    return decode_bool(value) ? std::string(text[0]) : std::string(text[1]);
  }

  static float decode_temperature(const std::string &data);
  static float decode_voltage(const std::string &data);
  static int decode_int(const std::string &data);
  static time_t decode_date(const std::string &data);
  static time_t decode_time(const std::string &data);

  static std::string decode_heater_el(const std::string &data) { return data; }
  static std::string decode_int_str(const std::string &data, const std::vector<std::string> &list);

 private:
  template<typename T> static std::optional<T> parse_data(const std::string &str);
};

}  // namespace esphome::fendt_caravan
#endif
