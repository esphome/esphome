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

#ifdef USE_ESP32
/// ESP-IDF waits on index 0 from the main task (e.g. pthread_join() uses an unfiltered
/// xTaskNotifyWait), so main-loop wakes on index 0 would end those waits early.
enum { ESPHOME_MAIN_TASK_NOTIFY_INDEX = 1 };
#if configTASK_NOTIFICATION_ARRAY_ENTRIES < 2
#error "CONFIG_FREERTOS_TASK_NOTIFICATION_ARRAY_ENTRIES must be at least 2"
#endif
#endif

/// Wake the main loop task from another FreeRTOS task. NOT ISR-safe.
/// always_inline so callers placed in IRAM do not reference a flash-resident copy.
__attribute__((always_inline)) static inline void esphome_main_task_notify() {
  TaskHandle_t task = esphome_main_task_handle;
  if (task != NULL) {
#ifdef USE_ESP32
    xTaskNotifyGiveIndexed(task, ESPHOME_MAIN_TASK_NOTIFY_INDEX);
#else
    xTaskNotifyGive(task);
#endif
  }
}

/// Wake the main loop task from an ISR. ISR-safe.
__attribute__((always_inline)) static inline void esphome_main_task_notify_from_isr(
    BaseType_t *px_higher_priority_task_woken) {
  TaskHandle_t task = esphome_main_task_handle;
  if (task != NULL) {
#ifdef USE_ESP32
    vTaskNotifyGiveIndexedFromISR(task, ESPHOME_MAIN_TASK_NOTIFY_INDEX, px_higher_priority_task_woken);
#else
    vTaskNotifyGiveFromISR(task, px_higher_priority_task_woken);
#endif
  }
}

/// Block the main loop task until it is woken or the timeout expires.
__attribute__((always_inline)) static inline void esphome_main_task_wait(TickType_t ticks) {
#ifdef USE_ESP32
  ulTaskNotifyTakeIndexed(ESPHOME_MAIN_TASK_NOTIFY_INDEX, pdTRUE, ticks);
#else
  ulTaskNotifyTake(pdTRUE, ticks);
#endif
}

#ifdef __cplusplus
}
#endif

#endif  // USE_ESP32 || USE_LIBRETINY
