#pragma once

#ifdef USE_ZEPHYR

#include <cstdint>

#include <zephyr/kernel.h>

#define IRAM_ATTR
#define PROGMEM

namespace esphome::zephyr {}

namespace esphome {

/// Returns true when executing inside an interrupt handler.
/// Zephyr/nRF52: not currently consulted — wake path is platform-specific.
__attribute__((always_inline)) inline bool in_isr_context() { return false; }

__attribute__((always_inline)) inline void yield() { ::k_yield(); }
__attribute__((always_inline)) inline void delay(uint32_t ms) { ::k_msleep(ms); }
__attribute__((always_inline)) inline uint32_t micros() { return k_ticks_to_us_floor32(k_uptime_ticks()); }
__attribute__((always_inline)) inline uint64_t millis_64() { return static_cast<uint64_t>(k_uptime_get()); }
__attribute__((always_inline)) inline uint32_t millis() { return static_cast<uint32_t>(millis_64()); }

// NOLINTNEXTLINE(readability-identifier-naming)
__attribute__((always_inline)) inline void delayMicroseconds(uint32_t us) { ::k_usleep(us); }
__attribute__((always_inline)) inline uint32_t arch_get_cpu_cycle_count() { return k_cycle_get_32(); }
__attribute__((always_inline)) inline uint32_t arch_get_cpu_freq_hz() { return sys_clock_hw_cycles_per_sec(); }

void arch_feed_wdt();
void arch_init();

}  // namespace esphome

#endif  // USE_ZEPHYR
