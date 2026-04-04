#pragma once

/// @file wake.h
/// Platform-specific main loop wake primitives.
/// Always available on all platforms — no opt-in needed.

#include "esphome/core/defines.h"
#include "esphome/core/hal.h"

#ifdef USE_LWIP_FAST_SELECT
#include "esphome/core/lwip_fast_select.h"
#ifdef USE_ESP32
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#else
#include <FreeRTOS.h>
#include <task.h>
#endif
#endif
#ifdef USE_ESP8266
#include <coredecls.h>
#elif defined(USE_RP2040)
#include <hardware/sync.h>
#include <pico/time.h>
#endif

namespace esphome {

// === Wake flag for ESP8266/RP2040 ===
// Checked by wakeable_delay() to exit early. Set by wake functions.
#if defined(USE_ESP8266) || defined(USE_RP2040)
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern volatile bool g_main_loop_woke;
#endif

// === ESP32/LibreTiny (FreeRTOS) ===
#ifdef USE_LWIP_FAST_SELECT

#ifdef USE_ESP32
/// Inline implementation — callers in IRAM get it inlined, keeping it IRAM-safe.
/// IRAM_ATTR entry points in wake.cpp exist for callers that aren't themselves IRAM.
inline void ESPHOME_ALWAYS_INLINE wake_loop_isrsafe_inline_(int *px_higher_priority_task_woken) {
  esphome_lwip_wake_main_loop_from_isr(px_higher_priority_task_woken);
}

/// IRAM_ATTR entry point — defined in wake.cpp.
void wake_loop_isrsafe(int *px_higher_priority_task_woken);

inline void ESPHOME_ALWAYS_INLINE wake_loop_any_context_inline_() { esphome_lwip_wake_main_loop_any_context(); }

/// IRAM_ATTR entry point — defined in wake.cpp.
void wake_loop_any_context();
#else
/// LibreTiny: no working IRAM_ATTR — just use threadsafe version.
inline void wake_loop_any_context() { esphome_lwip_wake_main_loop(); }
#endif

inline void wake_loop_threadsafe() { esphome_lwip_wake_main_loop(); }

namespace internal {
inline void wakeable_delay(uint32_t ms) {
  if (ms == 0) {
    yield();
    return;
  }
  ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(ms));
}
}  // namespace internal

// === ESP8266 ===
#elif defined(USE_ESP8266)

/// Inline implementation — IRAM callers inline this directly.
inline void ESPHOME_ALWAYS_INLINE wake_loop_impl_() {
  g_main_loop_woke = true;
  esp_schedule();
}

/// IRAM_ATTR entry point for ISR callers — defined in wake.cpp.
void wake_loop_any_context();

/// Non-ISR: always inline.
inline void wake_loop_threadsafe() { wake_loop_impl_(); }

namespace internal {
inline void wakeable_delay(uint32_t ms) {
  if (ms == 0) {
    delay(0);
    return;
  }
  g_main_loop_woke = false;
  esp_delay(ms, []() { return !g_main_loop_woke; });
}
}  // namespace internal

// === RP2040 ===
#elif defined(USE_RP2040)

inline void wake_loop_any_context() {
  g_main_loop_woke = true;
  __sev();
}

inline void wake_loop_threadsafe() {
  g_main_loop_woke = true;
  __sev();
}

namespace internal {
inline void wakeable_delay(uint32_t ms) {
  // Function-local statics — safe because this is only called from the main loop.
  static volatile bool s_delay_expired = false;
  if (ms == 0) {
    yield();
    return;
  }
  if (g_main_loop_woke) {
    g_main_loop_woke = false;
    return;
  }
  s_delay_expired = false;
  auto alarm_cb = [](alarm_id_t, void *) -> int64_t {
    s_delay_expired = true;
    __sev();
    return 0;
  };
  alarm_id_t alarm = add_alarm_in_ms(ms, alarm_cb, nullptr, true);
  if (alarm <= 0) {
    delay(ms);
    return;
  }
  while (!g_main_loop_woke && !s_delay_expired) {
    __wfe();
  }
  if (!s_delay_expired)
    cancel_alarm(alarm);
  g_main_loop_woke = false;
}
}  // namespace internal

// === Host (UDP loopback socket) ===
#else

/// Host platform: wakes select() via UDP loopback socket. Defined in application.cpp.
void wake_loop_threadsafe();

/// Host: no ISR, just use threadsafe version.
inline void wake_loop_any_context() { wake_loop_threadsafe(); }

#endif

}  // namespace esphome
