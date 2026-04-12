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
// pure 32-bit ops (subtract, add, compare-and-subtract). No __umulsidi3 software
// multiply calls.
//
// Overflow safety: system_get_time() is a uint32_t that wraps every ~71.6 minutes.
// Unsigned subtraction (now - last) handles one wrap correctly. ESPHome calls
// millis() thousands of times per second (1+N per loop iteration at 60+ Hz), so
// missing a full 71-minute wrap period is not a realistic concern. At boot,
// s_last_us starts at 0 and system_get_time() counts from 0, so the first call's
// delta equals the real elapsed time — no special initialization needed.
//
// This function is also installed as __wrap_millis (via -Wl,--wrap=millis) so
// that Arduino library code and ISR handlers (e.g. Wiegand, ZyAura) calling
// ::millis() directly also get the fast version. Interrupts are briefly disabled
// (~10 instructions, ~125 ns at 80 MHz) to protect the static state from
// concurrent ISR access.
static uint32_t IRAM_ATTR HOT millis_accumulator() {
  static uint32_t s_cache = 0;
  static uint32_t s_remainder = 0;
  static uint32_t s_last_us = 0;
  uint32_t ps = xt_rsil(15);
  uint32_t now_us = system_get_time();
  uint32_t delta = now_us - s_last_us;
  s_last_us = now_us;
  s_remainder += delta;
  while (s_remainder >= 1000) {
    s_cache++;
    s_remainder -= 1000;
  }
  uint32_t result = s_cache;
  xt_wsr_ps(ps);
  return result;
}
uint32_t IRAM_ATTR HOT millis() { return millis_accumulator(); }
uint64_t millis_64() { return Millis64Impl::compute(millis()); }
// Avoid calling ::delay() which pulls in __delay from core_esp8266_wiring.cpp.
// __delay has an intra-object call to the original millis() that --wrap=millis
// can't intercept, preventing the linker from garbage-collecting the expensive
// original millis body (~80 bytes IRAM). This yield loop achieves the same
// behavior: feeds the watchdog, processes SDK tasks, keeps WiFi alive.
void HOT delay(uint32_t ms) {
  uint32_t start = millis();
  while (millis() - start < ms) {
    yield();
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
extern "C" uint32_t IRAM_ATTR __wrap_millis() { return esphome::millis(); }

#endif  // USE_ESP8266
