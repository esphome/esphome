#pragma once

/// @file wake.h
/// Platform-specific main loop wake primitives.
/// Always available on all platforms — no opt-in needed.

#include "esphome/core/defines.h"
#include "esphome/core/hal.h"

#ifdef ESPHOME_THREAD_MULTI_ATOMICS
#include <atomic>
#endif

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

// === wake_request flag — signals Application::loop() that a producer queued
// work for some component's loop() to drain (MQTT RX, USB RX, BLE event, etc.)
// and the component phase should run this tick instead of being held off by
// the loop_interval_ gate. Set by every wake_loop_* entry point; consumed
// (via exchange-and-clear) at the gate in Application::loop(). ===
//
// std::atomic<uint8_t> rather than std::atomic<bool> because GCC on Xtensa
// generates an indirect function call for atomic<bool> ops instead of inlining
// them — same workaround applied in scheduler.h for the SchedulerItem::remove
// flag. On non-atomic platforms a volatile uint8_t suffices: 8-bit aligned
// loads/stores are atomic on every supported MCU, and the platform signal
// that follows wake_request_set() (FreeRTOS task-notify, esp_schedule, socket
// send) provides the cross-thread/cross-core memory barrier.
#ifdef ESPHOME_THREAD_MULTI_ATOMICS
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern std::atomic<uint8_t> g_wake_requested;

__attribute__((always_inline)) inline void wake_request_set() { g_wake_requested.store(1, std::memory_order_release); }
__attribute__((always_inline)) inline bool wake_request_take() {
  return g_wake_requested.exchange(0, std::memory_order_acquire) != 0;
}
#else
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern volatile uint8_t g_wake_requested;

__attribute__((always_inline)) inline void wake_request_set() { g_wake_requested = 1; }
__attribute__((always_inline)) inline bool wake_request_take() {
  uint8_t v = g_wake_requested;
  g_wake_requested = 0;
  return v != 0;
}
#endif

// === ESP32 / LibreTiny (FreeRTOS) ===
#if defined(USE_ESP32) || defined(USE_LIBRETINY)

/// Wake the main loop from any context (ISR or task).
/// always_inline so callers placed in IRAM keep the whole wake path in IRAM.
__attribute__((always_inline)) inline void wake_main_task_any_context() {
  // Set the wake-requested flag BEFORE the task notification so the consumer
  // (Application::loop() gate) is guaranteed to see it on its next gate check.
  wake_request_set();
  if (in_isr_context()) {
    BaseType_t px_higher_priority_task_woken = pdFALSE;
    esphome_main_task_notify_from_isr(&px_higher_priority_task_woken);
#ifdef portYIELD_FROM_ISR
    portYIELD_FROM_ISR(px_higher_priority_task_woken);
#else
    // ARM9 FreeRTOS port (BK72xx) does not define portYIELD_FROM_ISR; the IRQ
    // exit sequence performs the context switch if one was requested.
    (void) px_higher_priority_task_woken;
#endif
  } else {
    esphome_main_task_notify();
  }
}

/// IRAM_ATTR entry points — defined in wake.cpp.
void wake_loop_isrsafe(BaseType_t *px_higher_priority_task_woken);
void wake_loop_any_context();

inline void wake_loop_threadsafe() {
  wake_request_set();
  esphome_main_task_notify();
}

namespace internal {
inline void ESPHOME_ALWAYS_INLINE wakeable_delay(uint32_t ms) {
  // Fast path (with USE_LWIP_FAST_SELECT): FreeRTOS task notifications posted by the lwip
  // event_callback wrapper (see lwip_fast_select.c) are the single source of truth for
  // socket wake-ups. Every NETCONN_EVT_RCVPLUS posts an xTaskNotifyGive, so any notification
  // that lands between wakes keeps the counter non-zero (next ulTaskNotifyTake returns
  // immediately) or wakes a blocked Take directly. Additional wake sources:
  // wake_loop_threadsafe() from background tasks, and the ms timeout.
  if (ms == 0) [[unlikely]] {
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
  // Set the wake-requested flag BEFORE esp_schedule so the consumer is
  // guaranteed to see it on its next gate check.
  wake_request_set();
  g_main_loop_woke = true;
  esp_schedule();
}

/// IRAM_ATTR entry point for ISR callers — defined in wake.cpp.
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
    return;
  }
  esp_delay(ms, []() { return !g_main_loop_woke; });
}
}  // namespace internal

// === RP2040 ===
#elif defined(USE_RP2040)

inline void wake_loop_any_context() {
  // Set the wake-requested flag BEFORE the SEV so the consumer is guaranteed
  // to see it on its next gate check.
  wake_request_set();
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
inline void ESPHOME_ALWAYS_INLINE wakeable_delay(uint32_t ms) {
  if (ms == 0) [[unlikely]] {
    yield();
    return;
  }
  delay(ms);
}
}  // namespace internal

#endif

}  // namespace esphome
