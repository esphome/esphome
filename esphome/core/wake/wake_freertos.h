#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP32) || defined(USE_LIBRETINY)

#include "esphome/core/hal.h"
#include "esphome/core/main_task.h"

namespace esphome {

/// Wake the main loop from any context (ISR or task).
/// always_inline so callers placed in IRAM keep the whole wake path in IRAM.
__attribute__((always_inline)) inline void wake_main_task_any_context() {
  // Set the wake-requested flag BEFORE the task notification so the consumer
  // (Application::loop() gate) is guaranteed to see it on its next gate check.
  wake_request_set();
  if (in_isr_context()) {
    BaseType_t px_higher_priority_task_woken = pdFALSE;
    esphome_main_task_notify_from_isr(&px_higher_priority_task_woken);
#ifdef portYIELD_FROM_ISR
    portYIELD_FROM_ISR(px_higher_priority_task_woken);
#else
    // ARM9 FreeRTOS port (BK72xx) does not define portYIELD_FROM_ISR; the IRQ
    // exit sequence performs the context switch if one was requested.
    (void) px_higher_priority_task_woken;
#endif
  } else {
    esphome_main_task_notify();
  }
}

/// IRAM_ATTR entry points — defined in wake_freertos.cpp.
void wake_loop_isrsafe(BaseType_t *px_higher_priority_task_woken);
void wake_loop_any_context();

inline void wake_loop_threadsafe() {
  wake_request_set();
  esphome_main_task_notify();
}

namespace internal {
inline void ESPHOME_ALWAYS_INLINE wakeable_delay(uint32_t ms) {
  // Fast path (with USE_LWIP_FAST_SELECT): FreeRTOS task notifications posted by the lwip
  // event_callback wrapper (see lwip_fast_select.c) are the single source of truth for
  // socket wake-ups. Every NETCONN_EVT_RCVPLUS posts an xTaskNotifyGive, so any notification
  // that lands between wakes keeps the counter non-zero (next ulTaskNotifyTake returns
  // immediately) or wakes a blocked Take directly. Additional wake sources:
  // wake_loop_threadsafe() from background tasks, and the ms timeout.
  if (ms == 0) [[unlikely]] {
    yield();
    return;
  }
  ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(ms));
}
}  // namespace internal

}  // namespace esphome

#endif  // USE_ESP32 || USE_LIBRETINY
