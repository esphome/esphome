#ifdef USE_ESP8266

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

#include <Arduino.h>

extern "C" {
#include <user_interface.h>
}

// Empty esp8266 namespace block to satisfy ci-custom's lint_namespace check.
// HAL functions live in namespace esphome (root) — they are not part of the
// esp8266 component's API.
namespace esphome::esp8266 {}  // namespace esphome::esp8266

namespace esphome {

// yield(), delay(), micros(), millis(), millis_64(), delayMicroseconds(),
// arch_feed_wdt(), progmem_read_*() are inlined in components/esp8266/hal.h.
// delay() and millis() forward to the stock Arduino ESP8266 implementations.

void arch_restart() {
  system_restart();
  // restart() doesn't always end execution
  while (true) {  // NOLINT(clang-diagnostic-unreachable-code)
    yield();
  }
}

}  // namespace esphome

#endif  // USE_ESP8266
