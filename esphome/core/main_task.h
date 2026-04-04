#pragma once

/// Main loop task handle — shared between wake.h (C++) and lwip_fast_select.c (C).
/// Set once during Application::setup() via xTaskGetCurrentTaskHandle().

#ifdef USE_ESP32
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#elif defined(USE_LIBRETINY)
#include <FreeRTOS.h>
#include <task.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern TaskHandle_t esphome_main_task_handle;

#ifdef __cplusplus
}
#endif
