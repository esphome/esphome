#pragma once
// Single-threaded mutex double: every take succeeds
using SemaphoreHandle_t = int *;
inline constexpr int pdTRUE = 1;
inline SemaphoreHandle_t xSemaphoreCreateMutex() {
  static int mutex;
  return &mutex;
}
inline void vSemaphoreDelete(SemaphoreHandle_t /*mutex*/) {}
inline int xSemaphoreTake(SemaphoreHandle_t /*mutex*/, int /*ticks*/) { return pdTRUE; }
inline int xSemaphoreGive(SemaphoreHandle_t /*mutex*/) { return pdTRUE; }
