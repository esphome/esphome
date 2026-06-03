#pragma once

/// @file wake.h
/// Platform-specific main loop wake primitives.
/// Always available on all platforms — no opt-in needed.
///
/// The public API for callers lives here; the per-platform implementations
/// live under esphome/core/wake/ and are included at the bottom of this file
/// based on the active USE_* platform define.

#include "esphome/core/defines.h"
#include "esphome/core/hal.h"

#ifdef ESPHOME_THREAD_MULTI_ATOMICS
#include <atomic>
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

}  // namespace esphome

// Per-platform implementations. Each header re-enters namespace esphome {} and
// guards its body with the matching USE_* check, so only one contributes code
// for the active target.
#if defined(USE_ESP32) || defined(USE_LIBRETINY)
#include "esphome/core/wake/wake_freertos.h"
#elif defined(USE_ESP8266)
#include "esphome/core/wake/wake_esp8266.h"
#elif defined(USE_RP2040)
#include "esphome/core/wake/wake_rp2040.h"
#elif defined(USE_HOST)
#include "esphome/core/wake/wake_host.h"
#elif defined(USE_ZEPHYR)
#include "esphome/core/wake/wake_zephyr.h"
#else
#error "wake.h: wake_loop_threadsafe() is not implemented for this platform"
#endif
