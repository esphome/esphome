#pragma once

#ifdef USE_ESP32

#include <cstdint>
#include <esp_attr.h>
#include <esp_cpu.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "esphome/core/time_conversion.h"

#ifndef PROGMEM
#define PROGMEM
#endif

namespace esphome::esp32 {}

namespace esphome {

// Forward decl from helpers.h (esphome/core/helpers.h) — kept here so this
// header does not need to pull the rest of helpers.h.
// NOLINTNEXTLINE(readability-redundant-declaration)
void delay_microseconds_safe(uint32_t us);

/// Returns true when executing inside an interrupt handler.
__attribute__((always_inline)) inline bool in_isr_context() { return xPortInIsrContext() != 0; }

// Forward decl from <esp_timer.h>.
// NOLINTNEXTLINE(readability-redundant-declaration)
extern "C" int64_t esp_timer_get_time(void);

__attribute__((always_inline)) inline void yield() { vPortYield(); }
__attribute__((always_inline)) inline void delay(uint32_t ms) { vTaskDelay(ms / portTICK_PERIOD_MS); }
__attribute__((always_inline)) inline uint32_t micros() { return static_cast<uint32_t>(esp_timer_get_time()); }
uint32_t millis();
__attribute__((always_inline)) inline uint64_t millis_64() {
  return micros_to_millis<uint64_t>(static_cast<uint64_t>(esp_timer_get_time()));
}

// NOLINTNEXTLINE(readability-identifier-naming)
__attribute__((always_inline)) inline void delayMicroseconds(uint32_t us) { delay_microseconds_safe(us); }
__attribute__((always_inline)) inline void arch_feed_wdt() { esp_task_wdt_reset(); }
__attribute__((always_inline)) inline uint32_t arch_get_cpu_cycle_count() { return esp_cpu_get_cycle_count(); }

void arch_init();
uint32_t arch_get_cpu_freq_hz();

}  // namespace esphome

#endif  // USE_ESP32
