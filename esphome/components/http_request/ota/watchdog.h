#pragma once

#include "esphome/core/defines.h"

#include <cstdint>

namespace esphome {
namespace http_request {
namespace watchdog {

class WatchdogManager {
#ifdef USE_HTTP_REQUEST_OTA_WATCHDOG_TIMEOUT
 public:
  WatchdogManager();
  ~WatchdogManager();

 private:
  uint32_t get_timeout_();
  void set_timeout_(uint32_t timeout_ms);

  uint32_t saved_timeout_ms_{0};
#endif
};

}  // namespace watchdog
}  // namespace http_request
}  // namespace esphome
