#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP8266

#include "esphome/core/hal.h"

#include <coredecls.h>

namespace esphome {

/// Inline implementation — IRAM callers inline this directly.
inline void ESPHOME_ALWAYS_INLINE wake_loop_impl() {
  // Set the wake-requested flag BEFORE esp_schedule so the consumer is
  // guaranteed to see it on its next gate check.
  wake_request_set();
  g_main_loop_woke = true;
  esp_schedule();
}

/// IRAM_ATTR entry point for ISR callers — defined in wake_esp8266.cpp.
void wake_loop_any_context();

/// Non-ISR: always inline.
inline void wake_loop_threadsafe() { wake_loop_impl(); }

/// ISR-safe: no task_woken arg because ESP8266 has no FreeRTOS. Caller must be IRAM_ATTR.
inline void ESPHOME_ALWAYS_INLINE wake_loop_isrsafe() { wake_loop_impl(); }

namespace internal {
inline void ESPHOME_ALWAYS_INLINE wakeable_delay(uint32_t ms) {
  if (ms == 0) [[unlikely]] {
    delay(0);
    return;
  }
  if (g_main_loop_woke) {
    g_main_loop_woke = false;
    // Yield even on the already-woken fast path so callers in tight loops
    // (e.g. lwIP raw TCP wait_for_data_) make forward progress when ISRs
    // keep re-setting g_main_loop_woke between iterations.
    delay(0);
    return;
  }
  esp_delay(ms, []() { return !g_main_loop_woke; });
}
}  // namespace internal

}  // namespace esphome

#endif  // USE_ESP8266
