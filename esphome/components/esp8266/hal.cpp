#ifdef USE_ESP8266

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

#include <Arduino.h>
#include <core_esp8266_features.h>
#include <coredecls.h>

extern "C" {
#include <user_interface.h>
}

// Empty esp8266 namespace block to satisfy ci-custom's lint_namespace check.
// HAL functions live in namespace esphome (root) — they are not part of the
// esp8266 component's API.
namespace esphome::esp8266 {}  // namespace esphome::esp8266

namespace esphome {

// yield(), micros(), millis_64(), delayMicroseconds(), arch_feed_wdt(),
// progmem_read_*() are inlined in components/esp8266/hal.h.
//
// Fast accumulator replacement for Arduino's millis() (~3.3 μs via 4× 64-bit
// multiplies on the LX106). Tracks a running ms counter from 32-bit
// system_get_time() deltas using pure 32-bit ops. Installed as __wrap_millis
// (via -Wl,--wrap=millis) so Arduino libs and IRAM_ATTR ISR handlers (e.g.
// Wiegand, ZyAura) also get the fast version. xt_rsil(15) guards the static
// state against ISR re-entry; the critical section is bounded (≤10 while-loop
// iterations, ~100 ns on the common path, or a constant-time /1000 ~2.5 μs on
// the rare path — well under WiFi's ~10 μs ISR latency budget). NMIs (level
// >15) are not masked, but the ESP8266 SDK's NMI handlers don't call millis().
//
// system_get_time() wraps every ~71.6 min; unsigned (now_us - last_us) handles
// one wrap. The main loop calls millis() at 60+ Hz, so delta stays tiny — a
// >71 min block would trip the watchdog long before it could matter here.
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
    // Reuse ms instead of `remainder %= US_PER_MS` — `%` would compile to a
    // second __umodsi3 call on the LX106 (no hardware divide).
    state.remainder -= ms * US_PER_MS;
  } else {
    // Common path: small gap. At most ~10 iterations since remainder was
    // < threshold (10 ms) on entry and delta adds at most one more threshold
    // before exiting this branch.
    while (state.remainder >= US_PER_MS) {
      state.cache++;
      state.remainder -= US_PER_MS;
    }
  }
  uint32_t result = state.cache;
  xt_wsr_ps(ps);
  return result;
}

// Wait at least `ms` milliseconds while letting the SDK run. The 1-arg esp_delay()
// suspends the cont task via os_timer + esp_suspend so WiFi/SDK tasks run freely
// (which timing-sensitive interrupt-driven code like ESP8266 software-serial RX in
// fingerprint_grow depends on), but it is a single suspend: esp_suspend() resumes
// on ANY esp_schedule() (not just our delay timer) and no-ops entirely when the
// cont can't suspend, so a lone esp_delay(ms) can return well before `ms` elapses.
// Loop on the wrapped fast millis() until the full interval has actually passed so
// callers get at least the delay they asked for, matching Arduino ::delay(). Using
// the wrapped millis() keeps the slow Arduino millis() body out of IRAM (the
// --wrap=millis goal of #15662 is preserved); millis() is hardware-backed, so it
// still advances on the rare path where esp_delay() can't suspend and busy-waits.
void HOT delay(uint32_t ms) {
  if (ms == 0) {
    optimistic_yield(1000);
    return;
  }
  uint32_t start = millis();
  for (uint32_t elapsed = 0; elapsed < ms; elapsed = millis() - start)
    esp_delay(ms - elapsed);
}

void arch_restart() {
  system_restart();
  // restart() doesn't always end execution
  while (true) {  // NOLINT(clang-diagnostic-unreachable-code)
    yield();
  }
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
