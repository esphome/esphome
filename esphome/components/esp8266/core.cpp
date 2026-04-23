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
uint64_t millis_64() { return Millis64Impl::compute(millis()); }
// Poll-based delay that avoids ::delay() — Arduino's __delay has an intra-object
// call to the original millis() that --wrap can't intercept, so calling ::delay()
// would keep the slow Arduino millis body alive in IRAM. optimistic_yield still
// enters esp_schedule()/esp_suspend_within_cont() via yield(), so SDK tasks and
// WiFi run correctly. Theoretically less power-efficient than Arduino's
// os_timer-based delay() for long waits, but nearly all ESPHome delays are short
// (sensor/I²C/SPI settling in the 1–100 ms range) where the difference is
// negligible.
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
