#ifdef USE_ESP8266

#include "core.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/time_64.h"
#include "esphome/core/helpers.h"
#include "preferences.h"
#include <Arduino.h>
#include <core_esp8266_features.h>

extern "C" {
#include <user_interface.h>
}

namespace esphome {

void HOT yield() { ::yield(); }
// Arduino ESP8266's millis() uses 4× 64-bit multiplies with magic constants to
// convert system_get_time() → ms while tracking overflow (~3.3 μs per call on
// the LX106 which has no hardware multiply-high instruction). We replace it with
// a simple accumulator that tracks a running millis counter from μs deltas using
// pure 32-bit ops on the common path (subtract, add, compare-and-subtract).
// Large gaps (>10 ms) fall back to a constant-time /1000 conversion.
//
// Overflow safety: system_get_time() is a uint32_t that wraps every ~71.6 minutes.
// Unsigned subtraction (now - last) handles one wrap correctly. ESPHome calls
// millis() thousands of times per second (1+N per loop iteration at 60+ Hz), so
// missing a full 71-minute wrap period is not a realistic concern. At boot,
// state.last_us starts at 0 and system_get_time() counts from 0, so the first call's
// delta equals the real elapsed time — no special initialization needed.
//
// This function is also installed as __wrap_millis (via -Wl,--wrap=millis) so
// that Arduino library code and ISR handlers (e.g. Wiegand, ZyAura) calling
// ::millis() directly also get the fast version. Interrupts are briefly disabled
// to protect the static state from concurrent ISR access. The critical section
// is bounded: the common path (delta < 10 ms) runs at most 10 subtract-and-
// compare iterations (~100 ns). Large gaps (WiFi scan, boot) fall back to a
// constant-time multiply-by-reciprocal (~2.5 μs, rare).
// Threshold above which we use constant-time /1000 instead of the while loop.
// 10 ms means the while loop runs at most 10 iterations (~100 ns) on the
// common path, well within the WiFi stack's ~10 μs interrupt latency budget.
static constexpr uint32_t MILLIS_RARE_PATH_THRESHOLD_US = 10000;
static constexpr uint32_t US_PER_MS = 1000;

uint32_t IRAM_ATTR HOT millis() {
  // Struct packs the three statics so the compiler loads one base address
  // instead of three separate literal pool entries (saves ~8 bytes IRAM).
  static struct {
    uint32_t cache;
    uint32_t remainder;
    uint32_t last_us;
  } state = {0, 0, 0};
  uint32_t ps = xt_rsil(15);
  uint32_t now_us = system_get_time();
  uint32_t delta = now_us - state.last_us;
  state.last_us = now_us;
  state.remainder += delta;
  if (state.remainder >= MILLIS_RARE_PATH_THRESHOLD_US) {
    // Rare path: large gap (WiFi scan, boot, long block). Constant-time
    // conversion keeps the critical section bounded.
    uint32_t ms = state.remainder / US_PER_MS;
    state.cache += ms;
    state.remainder -= ms * US_PER_MS;
  } else {
    // Common path: small gap. Loop runs at most
    // MILLIS_RARE_PATH_THRESHOLD_US / US_PER_MS iterations.
    while (state.remainder >= US_PER_MS) {
      state.cache++;
      state.remainder -= US_PER_MS;
    }
  }
  uint32_t result = state.cache;
  xt_wsr_ps(ps);
  return result;
}
uint64_t millis_64() { return Millis64Impl::compute(millis()); }
// Avoid calling ::delay() which pulls in __delay from core_esp8266_wiring.cpp.
// __delay has an intra-object call to the original millis() that --wrap=millis
// can't intercept, preventing the linker from garbage-collecting the expensive
// original millis body (~80 bytes IRAM).
//
// Semantic difference from Arduino's delay(): Arduino sets up a one-shot
// os_timer and calls esp_suspend() to suspend the continuation once for the
// full duration. Our loop polls millis() + optimistic_yield(1000) which still
// calls esp_schedule()/esp_suspend_within_cont() via yield(), so SDK tasks
// and WiFi run correctly. Less power-efficient for long delays but
// functionally equivalent.
void HOT delay(uint32_t ms) {
  if (ms == 0) {
    optimistic_yield(1000);
    return;
  }
  uint32_t start = millis();
  while (millis() - start < ms) {
    optimistic_yield(1000);
  }
}
uint32_t IRAM_ATTR HOT micros() { return ::micros(); }
void IRAM_ATTR HOT delayMicroseconds(uint32_t us) { delay_microseconds_safe(us); }
void arch_restart() {
  system_restart();
  // restart() doesn't always end execution
  while (true) {  // NOLINT(clang-diagnostic-unreachable-code)
    yield();
  }
}
void arch_init() {}
void HOT arch_feed_wdt() { system_soft_wdt_feed(); }

uint8_t progmem_read_byte(const uint8_t *addr) {
  return pgm_read_byte(addr);  // NOLINT
}
const char *progmem_read_ptr(const char *const *addr) {
  return reinterpret_cast<const char *>(pgm_read_ptr(addr));  // NOLINT
}
uint16_t progmem_read_uint16(const uint16_t *addr) {
  return pgm_read_word(addr);  // NOLINT
}
uint32_t IRAM_ATTR HOT arch_get_cpu_cycle_count() { return esp_get_cycle_count(); }
uint32_t arch_get_cpu_freq_hz() { return F_CPU; }

void force_link_symbols() {
  // Tasmota uses magic bytes in the binary to check if an OTA firmware is compatible
  // with their settings - ESPHome uses a different settings system (that can also survive
  // erases). So set magic bytes indicating all tasmota versions are supported.
  // This only adds 12 bytes of binary size, which is an acceptable price to pay for easier support
  // for Tasmota.
  // https://github.com/arendst/Tasmota/blob/b05301b1497942167a015a6113b7f424e42942cd/tasmota/settings.ino#L346-L380
  // https://github.com/arendst/Tasmota/blob/b05301b1497942167a015a6113b7f424e42942cd/tasmota/i18n.h#L652-L654
  const static uint32_t TASMOTA_MAGIC_BYTES[] PROGMEM = {0x5AA55AA5, 0xFFFFFFFF, 0xA55AA55A};
  // Force link symbol by using a volatile integer (GCC attribute used does not work because of LTO)
  volatile int x = 0;
  x = TASMOTA_MAGIC_BYTES[x];
}

extern "C" void resetPins() {  // NOLINT
  // Added in framework 2.7.0
  // usually this sets up all pins to be in INPUT mode
  // however, not strictly needed as we set up the pins properly
  // ourselves and this causes pins to toggle during reboot.
  force_link_symbols();

#ifdef USE_ESP8266_EARLY_PIN_INIT
  for (int i = 0; i < 16; i++) {
    uint8_t mode = progmem_read_byte(&ESPHOME_ESP8266_GPIO_INITIAL_MODE[i]);
    uint8_t level = progmem_read_byte(&ESPHOME_ESP8266_GPIO_INITIAL_LEVEL[i]);
    if (mode != 255)
      pinMode(i, mode);  // NOLINT
    if (level != 255)
      digitalWrite(i, level);  // NOLINT
  }
#endif
}

}  // namespace esphome

// Linker wrap: redirect all ::millis() calls (Arduino libs, ISRs) to our accumulator.
// Requires -Wl,--wrap=millis in build flags (added by __init__.py).
// NOLINTNEXTLINE(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
extern "C" uint32_t IRAM_ATTR __wrap_millis() { return esphome::millis(); }
// Note: Arduino's init() registers a 60-second overflow timer for micros64().
// We leave it running — wrapping init() as a no-op would break micros64()'s
// overflow tracking, and the timer's cost is negligible (~3 μs per 60 s).

#endif  // USE_ESP8266
