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

void HOT yield() { ::yield(); }
// Arduino-pico's millis() uses time_us_64() (64-bit hardware timer read) then
// micros_to_millis() which does 64-bit multiply-shift conversion on an ARM
// Cortex-M0+ (~789 ns/call measured). Replace with a simple accumulator that
// tracks a running millis counter from 32-bit micros() deltas using pure
// 32-bit ops. ::micros() on RP2040 is a fast 32-bit hardware read (~220 ns).
//
// Overflow safety: ::micros() wraps every ~71.6 minutes. Unsigned subtraction
// handles one wrap correctly. ESPHome calls millis() thousands of times per
// second, so missing a full wrap is not a realistic concern. At boot, both
// s_last_us and ::micros() start at 0, so no special initialization needed.
//
// Also installed as __wrap_millis (via -Wl,--wrap=millis) so Arduino library
// code calling ::millis() directly gets the fast version. Interrupts are
// briefly disabled to protect the static state — Wiegand and ZyAura call
// millis() from ISR handlers on all platforms including RP2040.
static constexpr uint32_t MILLIS_RARE_PATH_THRESHOLD_US = 10000;
static constexpr uint32_t US_PER_MS = 1000;

uint32_t HOT millis() {
  static struct {
    uint32_t cache;
    uint32_t remainder;
    uint32_t last_us;
  } state = {0, 0, 0};
  uint32_t ps = save_and_disable_interrupts();
  uint32_t now_us = ::micros();
  uint32_t delta = now_us - state.last_us;
  state.last_us = now_us;
  state.remainder += delta;
  if (state.remainder >= MILLIS_RARE_PATH_THRESHOLD_US) {
    uint32_t ms = state.remainder / US_PER_MS;
    state.cache += ms;
    state.remainder -= ms * US_PER_MS;
  } else {
    while (state.remainder >= US_PER_MS) {
      state.cache++;
      state.remainder -= US_PER_MS;
    }
  }
  uint32_t result = state.cache;
  restore_interrupts(ps);
  return result;
}
// millis_64() keeps the full 64-bit timer for precision — called once per loop by Scheduler.
uint64_t millis_64() { return micros_to_millis<uint64_t>(time_us_64()); }
void HOT delay(uint32_t ms) { ::delay(ms); }
uint32_t HOT micros() { return ::micros(); }
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

// Linker wrap: redirect all ::millis() calls (Arduino libs) to our accumulator.
// Requires -Wl,--wrap=millis in build flags (added by __init__.py).
// NOLINTNEXTLINE(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
extern "C" uint32_t __wrap_millis() { return esphome::millis(); }

#endif  // USE_RP2040
