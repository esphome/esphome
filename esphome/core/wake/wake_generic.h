#pragma once

#include "esphome/core/defines.h"

#if !defined(USE_ESP32) && !defined(USE_LIBRETINY) && !defined(USE_ESP8266) && !defined(USE_RP2040) && \
    !defined(USE_HOST)

#include "esphome/core/hal.h"

namespace esphome {

#ifdef USE_ZEPHYR

/// Zephyr: wakes the main loop via k_sem_give(). Thread- and ISR-safe.
/// Defined in wake_generic.cpp.
void wake_loop_threadsafe();

inline void wake_loop_any_context() { wake_loop_threadsafe(); }

namespace internal {
/// Zephyr wakeable_delay uses k_sem_take() with a timeout — defined in wake_generic.cpp.
void wakeable_delay(uint32_t ms);
}  // namespace internal

#else

/// Fallback for any platform without a real wake mechanism.
/// wake_loop_threadsafe() is a no-op and wakeable_delay() falls back to delay().
inline void wake_loop_threadsafe() {}

inline void wake_loop_any_context() { wake_loop_threadsafe(); }

namespace internal {
inline void ESPHOME_ALWAYS_INLINE wakeable_delay(uint32_t ms) {
  if (ms == 0) [[unlikely]] {
    yield();
    return;
  }
  delay(ms);
}
}  // namespace internal

#endif  // USE_ZEPHYR

}  // namespace esphome

#endif  // fallback guard
