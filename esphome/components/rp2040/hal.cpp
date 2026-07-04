#ifdef USE_RP2040

#include "core.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#ifdef USE_RP2040_CRASH_HANDLER
#include "crash_handler.h"
#endif

#include "hardware/watchdog.h"

// Empty rp2040 namespace block to satisfy ci-custom's lint_namespace check.
// HAL functions live in namespace esphome (root) — they are not part of the
// rp2040 component's API.
namespace esphome::rp2040 {}  // namespace esphome::rp2040

namespace esphome {

// yield(), delay(), micros(), millis(), millis_64(), delayMicroseconds(),
// arch_feed_wdt(), arch_get_cpu_cycle_count() inlined in components/rp2040/hal.h.
void arch_restart() {
  watchdog_reboot(0, 0, 10);
  while (1) {
    continue;
  }
}

void arch_init() {
#ifdef USE_RP2040_CRASH_HANDLER
  rp2040::crash_handler_read_and_clear();
#endif
#if USE_RP2040_WATCHDOG_TIMEOUT > 0
  watchdog_enable(USE_RP2040_WATCHDOG_TIMEOUT, false);
#endif
}

uint32_t arch_get_cpu_freq_hz() { return RP2040::f_cpu(); }

}  // namespace esphome

#endif  // USE_RP2040
