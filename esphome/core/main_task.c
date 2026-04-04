#include "esphome/core/main_task.h"

#if defined(USE_ESP32) || defined(USE_LIBRETINY)

// IRAM_ATTR is defined by esp_attr.h (included via FreeRTOS headers) on ESP32.
// On LibreTiny it's not reliably available — provide a no-op fallback.
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

TaskHandle_t esphome_main_task_handle = NULL;

void esphome_main_task_notify(void) {
  TaskHandle_t task = esphome_main_task_handle;
  if (task != NULL) {
    xTaskNotifyGive(task);
  }
}

void IRAM_ATTR esphome_main_task_notify_from_isr(int *px_higher_priority_task_woken) {
  TaskHandle_t task = esphome_main_task_handle;
  if (task != NULL) {
    vTaskNotifyGiveFromISR(task, (BaseType_t *) px_higher_priority_task_woken);
  }
}

#ifdef USE_ESP32
void IRAM_ATTR esphome_main_task_notify_any_context(void) {
  if (xPortInIsrContext()) {
    int px_higher_priority_task_woken = 0;
    esphome_main_task_notify_from_isr(&px_higher_priority_task_woken);
    portYIELD_FROM_ISR(px_higher_priority_task_woken);
  } else {
    esphome_main_task_notify();
  }
}
#endif

#endif  // USE_ESP32 || USE_LIBRETINY
