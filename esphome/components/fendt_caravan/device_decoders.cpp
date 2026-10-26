#include "device_decoders.h"

#ifdef USE_ESP32
namespace esphome::fendt_caravan {
ESPHOME_LOG_TAG(TAG, "FC.decoders");

inline void trim(std::string &s) {
  // left trim
  s.erase(s.begin(), std::find_if_not(s.begin(), s.end(), [](unsigned char ch) { return std::isspace(ch); }));

  // right trim
  s.erase(std::find_if_not(s.rbegin(), s.rend(), [](unsigned char ch) { return std::isspace(ch); }).base(), s.end());
}

inline bool parse_data_int(const char *text, int &out) {
  char *end;
  out = static_cast<int>(strtol(text, &end, 10));
  return end != text;
}

float DeviceDecoders::decode_float(const char *data) {
  char buffer[128];
  if (strlen(data) >= sizeof(buffer) - 1) {
    ESP_LOGE(TAG, "Data too long for decode_float: '%s'", data);
    return 0.0f;
  }
  strcpy(buffer, data);
  for (char *p = buffer; *p != '\0'; p++) {
    if (*p == ',')
      *p = '.';
  }
  float out = strtof(data, nullptr);
  return out;
}

int DeviceDecoders::decode_int(const char *value) {
  char data[64];
  size_t len = strlen(value);
  if (len >= sizeof(data))
    len = sizeof(data) - 1;
  memcpy(data, value, len);
  data[len] = '\0';

  // left trim
  char *start = data;
  while (*start != '\0' && std::isspace(static_cast<unsigned char>(*start)))
    start++;
  // right trim
  char *end = data + strlen(data);
  while (end > start && std::isspace(static_cast<unsigned char>(*(end - 1))))
    end--;
  *end = '\0';

  // shift trimmed content to beginning
  size_t trimmed_len = end - start;
  memmove(data, start, trimmed_len + 1);

  int val;
  val = static_cast<int>(strtol(data, nullptr, 10));
  return val;
}

time_t DeviceDecoders::decode_date(const std::string &data) {
  std::istringstream date(data);
  tm tm = {};
  date >> std::get_time(&tm, "%d.%m.%y");
  if (date.fail()) {
    ESP_LOGE(TAG, "Date Parsing failed");
    return 0;
  }
  time_t ret = mktime(&tm);
  return ret;
}
time_t DeviceDecoders::decode_time(const std::string &data) {
  std::istringstream date(data);
  tm tm = {};
  date >> std::get_time(&tm, "%H:%M:%S");
  if (date.fail()) {
    ESP_LOGE(TAG, "Date Parsing failed");
    return 0;
  }
  time_t ret = mktime(&tm);
  return ret;
}
std::string DeviceDecoders::decode_int_str(const std::string &value, const std::vector<std::string> &list) {
  std::string data = value;
  trim(data);
  int val;
  auto result = parse_data_int(data.c_str(), val);
  if (!result) {
    ESP_LOGE(TAG, "Int list parse error. Data: '%s'", data.c_str());
    return "";
  }
  return list.at(val);
}

}  // namespace esphome::fendt_caravan
#endif
