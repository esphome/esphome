#pragma once

#ifdef USE_ESP8266

#include <c_types.h>
#include <core_esp8266_features.h>
#include <cstdint>
#include <pgmspace.h>

#include "esphome/core/time_64.h"

#ifndef PROGMEM
#define PROGMEM ICACHE_RODATA_ATTR
#endif

// Forward decls from Arduino's <Arduino.h> for the inline wrappers below.
// NOLINTBEGIN(google-runtime-int,readability-identifier-naming,readability-redundant-declaration)
extern "C" void yield(void);
extern "C" void delay(unsigned long ms);
extern "C" unsigned long micros(void);
extern "C" unsigned long millis(void);
// NOLINTEND(google-runtime-int,readability-identifier-naming,readability-redundant-declaration)

// Forward decl from <user_interface.h> for arch_feed_wdt() inline below.
// NOLINTNEXTLINE(readability-redundant-declaration)
extern "C" void system_soft_wdt_feed(void);

namespace esphome::esp8266 {}

namespace esphome {

// Forward decl from helpers.h so this header stays cheap.
// NOLINTNEXTLINE(readability-redundant-declaration)
void delay_microseconds_safe(uint32_t us);

/// Returns true when executing inside an interrupt handler.
/// ESP8266 has no reliable single-register ISR detection: PS.INTLEVEL is
/// non-zero both in a real ISR and when user code masks interrupts.  The
/// ESP8266 wake path is context-agnostic (wake_loop_impl uses esp_schedule
/// which is ISR-safe) so this helper is unused on this platform.
__attribute__((always_inline)) inline bool in_isr_context() { return false; }

__attribute__((always_inline)) inline void yield() { ::yield(); }
__attribute__((always_inline)) inline uint32_t micros() { return static_cast<uint32_t>(::micros()); }
void delay(uint32_t ms);
uint32_t millis();
__attribute__((always_inline)) inline uint64_t millis_64() { return Millis64Impl::compute(millis()); }

// ESP8266: pgm_read_* does aligned 32-bit flash reads on Harvard architecture.
// Inline-forward to the platform macros so the wrappers themselves don't
// occupy IRAM/flash on every call site.
__attribute__((always_inline)) inline uint8_t progmem_read_byte(const uint8_t *addr) {
  return pgm_read_byte(addr);  // NOLINT
}
__attribute__((always_inline)) inline const char *progmem_read_ptr(const char *const *addr) {
  return reinterpret_cast<const char *>(pgm_read_ptr(addr));  // NOLINT
}
__attribute__((always_inline)) inline uint16_t progmem_read_uint16(const uint16_t *addr) {
  return pgm_read_word(addr);  // NOLINT
}

// NOLINTNEXTLINE(readability-identifier-naming)
__attribute__((always_inline)) inline void delayMicroseconds(uint32_t us) { delay_microseconds_safe(us); }
__attribute__((always_inline)) inline void arch_feed_wdt() { system_soft_wdt_feed(); }
__attribute__((always_inline)) inline void arch_init() {}
// esp_get_cycle_count() declared in <core_esp8266_features.h>; F_CPU is a
// compiler-driven macro from the ESP8266 Arduino board defs (-DF_CPU=...).
__attribute__((always_inline)) inline uint32_t arch_get_cpu_cycle_count() { return esp_get_cycle_count(); }
__attribute__((always_inline)) inline uint32_t arch_get_cpu_freq_hz() { return F_CPU; }

}  // namespace esphome

#endif  // USE_ESP8266
