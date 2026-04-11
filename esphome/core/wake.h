#pragma once

/// @file wake.h
/// Platform-specific main loop wake primitives.
/// Always available on all platforms — no opt-in needed.
///
/// This file has two sections:
///   1. A C-compatible section at the top (the inline OTA wake hook) that .c files
///      like lwip_fast_select.c can include. Uses only <stdbool.h>/<stddef.h>.
///   2. A C++ section (everything after #ifdef __cplusplus below) with the existing
///      platform wake primitives and wakeable_delay() implementations.

// ============================================================================
// C-compatible section: inline OTA wake hook
// ============================================================================
//
// Called from lwip_fast_select.c (and lwip_raw_tcp_impl.cpp / host select) on every
// NETCONN_EVT_RCVPLUS so a disabled OTA loop can be re-enabled when a monitored
// socket signals activity. Static inline (not an extern-C trampoline) so the .c
// callback pays zero function-call overhead — just a pointer load, null check, and
// two volatile bool stores inlined into the callback body.
//
// The two pointers are captured once in Application::set_ota_wake_component(): they
// point at Component::pending_enable_loop_ and Application::has_pending_enable_loop_requests_.
// When OTA is not compiled into the build, nothing calls set_ota_wake_component(),
// the pointers stay NULL, and the inline collapses to a single null check.

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Address of the registered OTA component's pending_enable_loop_ flag, captured at
// registration time. NULL when no OTA component has registered (including when OTA
// is not compiled in at all).
extern volatile bool *esphome_ota_pending_enable_loop_ptr;
// Address of Application::has_pending_enable_loop_requests_. Set in tandem with the
// pending_enable pointer above — single null check covers both.
extern volatile bool *esphome_ota_has_pending_requests_ptr;

// Mark the registered OTA component pending loop-enable. Safe from the LwIP TCP/IP
// task and raw-TCP IRQ context — only volatile stores, no locks. Callers MUST invoke
// this BEFORE waking the main task, so the flags are visible on the next iteration.
static inline void esphome_wake_ota_component_any_context(void) {
  volatile bool *pending_enable = esphome_ota_pending_enable_loop_ptr;
  if (pending_enable != NULL) {
    *pending_enable = true;
    *esphome_ota_has_pending_requests_ptr = true;
  }
}

#ifdef __cplusplus
}  // extern "C"
#endif

// ============================================================================
// C++ section: platform wake primitives
// ============================================================================

#ifdef __cplusplus

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

#endif  // __cplusplus
