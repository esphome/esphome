#pragma once

/// Main loop task handle and wake helpers — shared between wake.h (C++) and lwip_fast_select.c (C).
/// esphome_main_task_handle is set once during Application::setup() via xTaskGetCurrentTaskHandle().

#if defined(USE_ESP32) || defined(USE_LIBRETINY)

#ifdef USE_ESP32
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#else
#include <FreeRTOS.h>
#include <task.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern TaskHandle_t esphome_main_task_handle;

/// Wake the main loop task from another FreeRTOS task. NOT ISR-safe.
/// always_inline so callers placed in IRAM do not reference a flash-resident copy.
__attribute__((always_inline)) static inline void esphome_main_task_notify() {
  TaskHandle_t task = esphome_main_task_handle;
  if (task != NULL) {
    xTaskNotifyGive(task);
  }
}

/// Wake the main loop task from an ISR. ISR-safe.
__attribute__((always_inline)) static inline void esphome_main_task_notify_from_isr(
    BaseType_t *px_higher_priority_task_woken) {
  TaskHandle_t task = esphome_main_task_handle;
  if (task != NULL) {
    vTaskNotifyGiveFromISR(task, px_higher_priority_task_woken);
  }
}

#ifdef __cplusplus
}
#endif

#endif  // USE_ESP32 || USE_LIBRETINY
