#pragma once

#ifdef USE_ESP32
#include "esphome/core/string_ref.h"
#include "esphome/core/log.h"

namespace esphome::fendt_caravan {

class Commands {
 public:
  static std::string update_toggle(const std::string &name, bool value = true) {
    std::string command = "cmd-tgl:" + name;
    return command;
  }
  static std::string update_run(const std::string &name, bool value = true) {
    std::string ret_value;
    if (name == "HS_KEY") {
      ret_value = "net-" + name;
    } else if (name == "HS_KEY_LONG") {
      ret_value = "net-" + name;
    } else {
      ret_value = std::string("cmd-run:") + name;
    }
    return ret_value;
  }

  static std::string update_temp_10(const std::string &name, float temp) {
    uint16_t value = (uint16_t) (temp * 10.0);
    char buf[6];
    snprintf(buf, sizeof(buf), "%" PRIu16, value);
    return "net-" + name + "-" + std::string(buf);
  }

  static std::string update_heater_el(const std::string &name, const std::string &el) {
    std::string value = "0";
    if (el == "1 kW") {
      value = "1";
    } else if (el == "2 kW") {
      value = "2";
    } else if (el == "3 kW") {
      value = "3";
    }
    return "net-" + name + "-" + value;
  }
  static std::string update_int(const std::string &name, uint16_t value) {
    char buf[6];
    snprintf(buf, sizeof(buf), "%" PRIu16, value);
    return "cmd-set:" + name + "=" + std::string(buf);
  }
};

}  // namespace esphome::fendt_caravan
#endif
