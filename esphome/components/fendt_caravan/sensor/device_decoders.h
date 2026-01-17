#pragma once
#include <ctime>
#include <iostream>
#include <iomanip>
#include "esphome/core/string_ref.h"

namespace esphome {
namespace fendt_caravan {
using namespace std;

class Coders {
 public:
  static bool decode_bool(const std::string &value) {
    std::string decode = value;
    std::transform(decode.begin(), decode.end(), decode.begin(), [](unsigned char c) { return std::tolower(c); });
    if ((decode == "01") || (decode == "true") || (decode == "1") || (decode == "on"))
      return true;
    else
      return false;
  }
  static std::string decode_str(const std::string &value) { return value; }
  static std::string decode_bool_str(const std::string &value, const char **text) {
    return decode_bool(value) ? std::string(text[0]) : std::string(text[1]);
  }
  static float decode_temperature(const std::string &data) {
    std::string value = data;
    size_t start = value.find("^C");
    if (start != std::string::npos)
      value.replace(start, 2, "");
    start = value.find(",");
    if (start != std::string::npos)
      value.replace(start, 1, ".");
    return std::stof(value);
  }
  static float decode_voltage(const std::string &data) {
    std::string value = data;
    size_t start = value.find("V");
    if (start != std::string::npos)
      value.replace(start, 1, "");
    start = value.find(",");
    if (start != std::string::npos)
      value.replace(start, 1, ".");
    return std::stof(value);
  }
  static int decode_int(const std::string &data) { return std::stoi(data); }
  static time_t decode_date(const std::string &data) {
    std::istringstream date(data);
    tm tm = {};
    date >> get_time(&tm, "%d.%m.%y");
    if (date.fail()) {
      ESP_LOGE("CODERS", "Date Parsing failed");
      return 0;
    }
    time_t ret = mktime(&tm);
    return ret;
  }
  static time_t decode_time(const std::string &data) {
    std::istringstream date(data);
    tm tm = {};
    date >> get_time(&tm, "%H:%M:%S");
    if (date.fail()) {
      ESP_LOGE("CODERS", "Date Parsing failed");
      return 0;
    }
    time_t ret = mktime(&tm);
    return ret;
  }

  static std::string decode_heater_el(const std::string &data) {
    std::string ret_val = data;

    /*     if( data == "Off" ) ret_val = "0";
        if( data == "1 kW") ret_val = "1";
        if( data == "2 kW") ret_val = "2";
        if( data == "3 kW") ret_val = "3"; */
    return ret_val;
  }
  static std::string decode_int_str(const std::string &data, const std::vector<std::string> &list) {
    int val = std::stoi(data);
    return list.at(val);
  }

 private:
};

}  // namespace fendt_caravan
}  // namespace esphome
