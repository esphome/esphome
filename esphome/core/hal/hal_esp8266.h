#pragma once

#ifdef USE_ESP8266

#include <c_types.h>
#include <cstdint>

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

namespace esphome {

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

// ESP8266: pgm_read_* does real flash reads on Harvard architecture
uint8_t progmem_read_byte(const uint8_t *addr);
const char *progmem_read_ptr(const char *const *addr);
uint16_t progmem_read_uint16(const uint16_t *addr);

}  // namespace esphome

#endif  // USE_ESP8266
