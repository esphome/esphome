#pragma once

#include "esphome/core/defines.h"

#include <cstdint>

namespace esphome::watchdog {

class WatchdogManager {
 public:
  WatchdogManager(uint32_t timeout_ms);
  ~WatchdogManager();

 private:
  uint32_t get_timeout_();
  void set_timeout_(uint32_t timeout_ms);

  uint32_t saved_timeout_ms_{0};
  uint32_t timeout_ms_{0};
};

}  // namespace esphome::watchdog
