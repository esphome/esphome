#pragma once

#include "FreeRTOS.h"

/// A distinct handle per thread, as each task has its own.
inline TaskHandle_t xTaskGetCurrentTaskHandle() {
  static thread_local char marker;
  return &marker;
}
inline void xTaskNotifyGiveIndexed(TaskHandle_t, int) {}
inline void vTaskNotifyGiveIndexedFromISR(TaskHandle_t, int, BaseType_t *) {}
inline unsigned ulTaskNotifyTakeIndexed(int, int, TickType_t) { return 0; }
