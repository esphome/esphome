#pragma once

#ifdef USE_ESP32
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <string>

namespace esphome::fendt_caravan {

class Commands {
 public:
  static std::string update_toggle(const std::string &name, bool value = true) {
    std::string command = "cmd-tgl:" + name;
    return command;
  }
  static std::string update_run(const std::string &name, bool value = true) {
    std::string ret_value;
    if (name == "HS_KEY" || name == "HS_KEY_LONG") {
      ret_value = "net-" + name;
    } else {
      ret_value = std::string("cmd-run:") + name;
    }
    return ret_value;
  }

  static std::string update_int(const std::string &name, uint16_t value) {
    char buf[6];
    snprintf(buf, sizeof(buf), "%" PRIu16, value);
    return "cmd-set:" + name + "=" + std::string(buf);
  }
};

}  // namespace esphome::fendt_caravan
#endif
