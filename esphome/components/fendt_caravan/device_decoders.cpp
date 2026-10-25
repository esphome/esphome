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
inline bool parse_data_float(const char *text, float &out) {
  char *data = const_cast<char *>(text);

  for (char *p = data; *p != '\0'; p++) {
    if (*p == ',')
      *p = '.';
  }
  char *end;
  out = strtof(text, &end);
  return end != text;
}

inline bool parse_data_int(const char *text, int &out) {
  char *end;
  out = static_cast<int>(strtol(text, &end, 10));
  return end != text;
}

float DeviceDecoders::decode_temperature(const std::string &data) {
  std::string value = data;
  size_t start = value.find("^C");
  if (start != std::string::npos)
    value.replace(start, 2, "");
  start = value.find(',');
  if (start != std::string::npos)
    value.replace(start, 1, ".");
  return std::stof(value);
}
float DeviceDecoders::decode_voltage(const std::string &data) {
  std::string value = data;
  size_t start = value.find('V');
  if (start != std::string::npos)
    value.replace(start, 1, "");
  start = value.find(',');
  if (start != std::string::npos)
    value.replace(start, 1, ".");
  trim(value);
  float val;
  auto result = parse_data_float(value.c_str(), val);
  if (!result) {
    ESP_LOGE(TAG, "Voltage parse error. Data: '%s'", value.c_str());
    return 0.0f;
  }
  return val;
}

int DeviceDecoders::decode_int(const std::string &value) {
  std::string data = value;
  trim(data);
  int val;
  auto result = parse_data_int(data.c_str(), val);
  if (!result) {
    ESP_LOGE(TAG, "Int parse error. Data: '%s'", data.c_str());
    return 0;
  }
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
