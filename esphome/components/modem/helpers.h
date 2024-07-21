#pragma once

#ifdef USE_ESP_IDF

#include "modem_component.h"

#include <cxx_include/esp_modem_api.hpp>

namespace esphome {
namespace modem {

std::string command_result_to_string(command_result err);

std::string state_to_string(ModemComponentState state);

// While instanciated, will set the WDT to timeout_s
// When deleted, will restore default WDT
class Watchdog {
 public:
  Watchdog(u_int32_t timeout_s);

  ~Watchdog();

 private:
  uint32_t timeout_s_;
  uint64_t start_time_ms_;

  void set_wdt_(uint32_t timeout_s);
};

}  // namespace modem
}  // namespace esphome
#endif  // USE_ESP_IDF
