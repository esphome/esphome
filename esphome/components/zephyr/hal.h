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

inline k_tid_t &main_loop_thread() {
  static k_tid_t thread = nullptr;
  return thread;
}

/// arch_init() stores the thread before any component runs, so no unset case exists.
__attribute__((always_inline)) inline bool is_main_loop_thread() { return k_current_get() == main_loop_thread(); }

__attribute__((always_inline)) inline void yield() { ::k_yield(); }
__attribute__((always_inline)) inline void delay(uint32_t ms) { ::k_msleep(ms); }
#if CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC == 1000000
// The system timer counts microseconds (the GRTC on nRF54L) and wraps at 2^32 like micros().
__attribute__((always_inline)) inline uint32_t micros() { return k_cycle_get_32(); }
#elif defined(CONFIG_SOC_SERIES_NRF52X) || defined(CONFIG_SOC_SERIES_NRF52)  // the name changed in sdk-nrf 3
#define USE_ZEPHYR_DWT_MICROS
uint32_t micros();  // CPU cycles kept close to the kernel clock, see hal.cpp
#else
__attribute__((always_inline)) inline uint32_t micros() { return k_ticks_to_us_floor32(k_uptime_ticks()); }
#endif
__attribute__((always_inline)) inline uint64_t millis_64() { return static_cast<uint64_t>(k_uptime_get()); }
__attribute__((always_inline)) inline uint32_t millis() { return static_cast<uint32_t>(millis_64()); }

// NOLINTNEXTLINE(readability-identifier-naming)
__attribute__((always_inline)) inline void delayMicroseconds(uint32_t us) { ::k_busy_wait(us); }
__attribute__((always_inline)) inline uint32_t arch_get_cpu_cycle_count() { return k_cycle_get_32(); }
__attribute__((always_inline)) inline uint32_t arch_get_cpu_freq_hz() { return sys_clock_hw_cycles_per_sec(); }

void arch_feed_wdt();
void arch_init();

}  // namespace esphome

#endif  // USE_ZEPHYR
