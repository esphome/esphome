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
  static bool decode_bool(const char *value) {
    char decoded[16];
    size_t i = 0;
    for (; value[i] != '\0' && i < sizeof(decoded) - 1; i++)
      decoded[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(value[i])));
    decoded[i] = '\0';
    return strcmp(decoded, "01") == 0 || strcmp(decoded, "true") == 0 || strcmp(decoded, "1") == 0 ||
           strcmp(decoded, "on") == 0;
  }

  static float decode_float(const char *data);
  static int decode_int(const char *value);
  static time_t decode_date(const std::string &data);
  static time_t decode_time(const std::string &data);

  static std::string decode_heater_el(const std::string &data) { return data; }
  static std::string decode_int_str(const std::string &data, const std::vector<std::string> &list);
};

}  // namespace esphome::fendt_caravan
#endif
