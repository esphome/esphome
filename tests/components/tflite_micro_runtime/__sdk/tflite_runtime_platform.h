#pragma once

// The FreeRTOS names the runtime uses. They are not provided as freertos/ headers: every suite's
// include directory is on one path, and other suites double those headers differently.
using TaskHandle_t = void *;

/// A distinct handle per thread, as each task has its own.
inline TaskHandle_t xTaskGetCurrentTaskHandle() {
  static thread_local char marker;
  return &marker;
}
