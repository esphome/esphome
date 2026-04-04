#pragma once

/// @file wake.h
/// Platform-specific main loop wake primitives.
/// Always available on all platforms — no opt-in needed.

#include "esphome/core/defines.h"
#include "esphome/core/hal.h"

#if defined(USE_ESP32) || defined(USE_LIBRETINY)
#include "esphome/core/main_task.h"
#endif
#ifdef USE_ESP8266
#include <coredecls.h>
#elif defined(USE_RP2040)
#include <hardware/sync.h>
#include <pico/time.h>
#endif

namespace esphome {

// === Wake flag for ESP8266/RP2040 ===
#if defined(USE_ESP8266) || defined(USE_RP2040)
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern volatile bool g_main_loop_woke;
#endif

// === ESP32 / LibreTiny (FreeRTOS) ===
#if defined(USE_ESP32) || defined(USE_LIBRETINY)

#ifdef USE_ESP32
/// IRAM_ATTR entry point — defined in wake.cpp.
void wake_loop_isrsafe(BaseType_t *px_higher_priority_task_woken);
/// IRAM_ATTR entry point — defined in wake.cpp.
void wake_loop_any_context();
#else
/// LibreTiny: IRAM_ATTR is not functional and the FreeRTOS port does not
/// provide vTaskNotifyGiveFromISR/portYIELD_FROM_ISR, so ISR-safe wake
/// is not possible. xTaskNotifyGive is used as the best available option.
inline void wake_loop_any_context() { esphome_main_task_notify(); }
#endif

inline void wake_loop_threadsafe() { esphome_main_task_notify(); }

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
inline void ESPHOME_ALWAYS_INLINE wake_loop_impl() {
  g_main_loop_woke = true;
  esp_schedule();
}

/// IRAM_ATTR entry point for ISR callers — defined in wake.cpp.
void wake_loop_any_context();

/// Non-ISR: always inline.
inline void wake_loop_threadsafe() { wake_loop_impl(); }

namespace internal {
inline void wakeable_delay(uint32_t ms) {
  if (ms == 0) {
    delay(0);
    return;
  }
  if (g_main_loop_woke) {
    g_main_loop_woke = false;
    return;
  }
  esp_delay(ms, []() { return !g_main_loop_woke; });
}
}  // namespace internal

// === RP2040 ===
#elif defined(USE_RP2040)

inline void wake_loop_any_context() {
  g_main_loop_woke = true;
  __sev();
}

inline void wake_loop_threadsafe() { wake_loop_any_context(); }

/// RP2040 wakeable delay uses file-scope state (alarm callback + flag) — defined in wake.cpp.
namespace internal {
void wakeable_delay(uint32_t ms);
}  // namespace internal

// === Host / Zephyr / other ===
#else

#ifdef USE_HOST
/// Host: wakes select() via UDP loopback socket. Defined in wake.cpp.
void wake_loop_threadsafe();
#else
/// Zephyr is currently the only platform without a wake mechanism.
/// wake_loop_threadsafe() is a no-op and wakeable_delay() falls back to delay().
/// TODO: implement proper Zephyr wake using k_poll / k_sem or similar.
inline void wake_loop_threadsafe() {}
#endif

inline void wake_loop_any_context() { wake_loop_threadsafe(); }

namespace internal {
inline void wakeable_delay(uint32_t ms) {
  if (ms == 0) {
    yield();
    return;
  }
  delay(ms);
}
}  // namespace internal

#endif

}  // namespace esphome
