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
void esphome_main_task_notify(void);

/// Wake the main loop task from an ISR. ISR-safe.
void esphome_main_task_notify_from_isr(int *px_higher_priority_task_woken);

#ifdef USE_ESP32
/// Wake the main loop from any context (ISR or task). ESP32-only (needs xPortInIsrContext).
void esphome_main_task_notify_any_context(void);
#endif

#ifdef __cplusplus
}
#endif

#endif  // USE_ESP32 || USE_LIBRETINY
