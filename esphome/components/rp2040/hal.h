#pragma once

#ifdef USE_RP2040

#include <cstdint>

#include "esphome/core/time_conversion.h"

#define IRAM_ATTR __attribute__((noinline, long_call, section(".time_critical")))
#define PROGMEM

// Forward decls from Arduino's <Arduino.h> for the inline wrappers below.
// NOLINTBEGIN(google-runtime-int,readability-identifier-naming,readability-redundant-declaration)
extern "C" void yield(void);
extern "C" void delay(unsigned long ms);
extern "C" unsigned long micros(void);
extern "C" unsigned long millis(void);
// NOLINTEND(google-runtime-int,readability-identifier-naming,readability-redundant-declaration)

// Forward decl from <pico/time.h>.
extern "C" uint64_t time_us_64(void);

// Forward decls from pico-sdk / FreeRTOS port for the inline arch_*
// wrappers below.
extern "C" void watchdog_update(void);
extern "C" unsigned long ulMainGetRunTimeCounterValue(void);

namespace esphome::rp2040 {}

namespace esphome {

// Forward decl from helpers.h.
// NOLINTNEXTLINE(readability-redundant-declaration)
void delay_microseconds_safe(uint32_t us);

/// Returns true when executing inside an interrupt handler.
__attribute__((always_inline)) inline bool in_isr_context() {
  uint32_t ipsr;
  __asm__ volatile("mrs %0, ipsr" : "=r"(ipsr));
  return ipsr != 0;
}

__attribute__((always_inline)) inline void yield() { ::yield(); }
__attribute__((always_inline)) inline void delay(uint32_t ms) { ::delay(ms); }
__attribute__((always_inline)) inline uint32_t micros() { return static_cast<uint32_t>(::micros()); }
__attribute__((always_inline)) inline uint32_t millis() { return micros_to_millis(::time_us_64()); }
__attribute__((always_inline)) inline uint64_t millis_64() { return micros_to_millis<uint64_t>(::time_us_64()); }

// NOLINTNEXTLINE(readability-identifier-naming)
__attribute__((always_inline)) inline void delayMicroseconds(uint32_t us) { delay_microseconds_safe(us); }
__attribute__((always_inline)) inline void arch_feed_wdt() { watchdog_update(); }
__attribute__((always_inline)) inline uint32_t arch_get_cpu_cycle_count() {
  return static_cast<uint32_t>(ulMainGetRunTimeCounterValue());
}

void arch_init();
uint32_t arch_get_cpu_freq_hz();

}  // namespace esphome

#endif  // USE_RP2040
