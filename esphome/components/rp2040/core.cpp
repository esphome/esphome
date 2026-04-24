#ifdef USE_RP2040

#include "core.h"
#include "esphome/core/defines.h"
#ifdef USE_RP2040_CRASH_HANDLER
#include "crash_handler.h"
#endif
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

#include "hardware/timer.h"
#include "hardware/watchdog.h"

namespace esphome {

// yield(), delay(), micros(), millis(), millis_64() inlined in hal.h.
void HOT delayMicroseconds(uint32_t us) { delay_microseconds_safe(us); }
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

void HOT arch_feed_wdt() { watchdog_update(); }

uint32_t HOT arch_get_cpu_cycle_count() { return ulMainGetRunTimeCounterValue(); }
uint32_t arch_get_cpu_freq_hz() { return RP2040::f_cpu(); }

}  // namespace esphome

#endif  // USE_RP2040
