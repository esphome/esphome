#pragma once

#ifdef USE_HOST

#include <cstdint>
#include <pthread.h>
#include <sched.h>

#define IRAM_ATTR
#define PROGMEM

namespace esphome::host {}

namespace esphome {

/// Returns true when executing inside an interrupt handler.
/// Host has no ISR concept.
__attribute__((always_inline)) inline bool in_isr_context() { return false; }

inline pthread_t &main_loop_thread() {
  static pthread_t thread{};
  return thread;
}

/// arch_init() stores the thread before any component runs, so no unset case exists.
__attribute__((always_inline)) inline bool is_main_loop_thread() {
  return pthread_equal(pthread_self(), main_loop_thread()) != 0;
}

__attribute__((always_inline)) inline void yield() { ::sched_yield(); }

void delay(uint32_t ms);
uint32_t micros();
uint32_t millis();
uint64_t millis_64();
void delayMicroseconds(uint32_t us);  // NOLINT(readability-identifier-naming)
uint32_t arch_get_cpu_cycle_count();

__attribute__((always_inline)) inline void arch_init() { main_loop_thread() = pthread_self(); }
__attribute__((always_inline)) inline void arch_feed_wdt() {}
__attribute__((always_inline)) inline uint32_t arch_get_cpu_freq_hz() { return 1000000000U; }

}  // namespace esphome

#endif  // USE_HOST
