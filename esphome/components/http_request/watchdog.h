#pragma once

#include "esphome/core/defines.h"

#include <cstdint>

namespace esphome {
namespace http_request {
namespace watchdog {

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

}  // namespace watchdog
}  // namespace http_request
}  // namespace esphome
