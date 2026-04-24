#pragma once

#include "esphome/core/defines.h"

#if !defined(USE_ESP32) && !defined(USE_LIBRETINY) && !defined(USE_ESP8266) && !defined(USE_RP2040) && \
    !defined(USE_HOST)

#include "esphome/core/hal.h"

namespace esphome {

/// Zephyr is currently the only platform without a wake mechanism.
/// wake_loop_threadsafe() is a no-op and wakeable_delay() falls back to delay().
/// TODO: implement proper Zephyr wake using k_poll / k_sem or similar.
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

}  // namespace esphome

#endif  // fallback guard
