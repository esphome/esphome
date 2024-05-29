#pragma once

#include <cstdint>

namespace esphome {
namespace http_request {
namespace watchdog {

static const char *const TAG = "watchdog.http_request.ota";

class Watchdog {
 public:
  static uint32_t get_timeout();
  static void set_timeout(uint32_t timeout_ms);
  static void reset();

 private:
  static uint32_t timeout_ms;       // NOLINT
  static uint32_t init_timeout_ms;  // NOLINT
  Watchdog() {}
};

class WatchdogSupervisor {
 public:
  WatchdogSupervisor();
  ~WatchdogSupervisor();
};

}  // namespace watchdog
}  // namespace http_request
}  // namespace esphome
