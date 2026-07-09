#pragma once

#ifdef USE_ESP32

#include "esphome/core/helpers.h"
#include "audio_core_log_utils.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace esphome::esp_audio_stack {

/// Pin a FreeRTOS task to a core, with optional static stack from PSRAM.
///
/// Two creation paths are folded into one call so callers do not have to
/// repeat the if/else. Both paths log the failure reason via the provided
/// `log_tag`.
///
///  - `psram_stack=true`: allocate `stack_bytes` worth of StackType_t storage
///    from PSRAM, then call xTaskCreateStaticPinnedToCore.
///    On allocation failure, returns false without touching the task system.
///    On success, *stack_out holds the allocated stack pointer.
///
///  - `psram_stack=false`: classic xTaskCreatePinnedToCore with FreeRTOS
///    managing the stack on the internal heap. *stack_out stays nullptr.
///
/// `tcb_out` and `stack_out` are only touched when `psram_stack=true`.
/// `handle_out` is always set (to the new handle, or to nullptr on failure).
///
/// Returns true if the task is alive and pinned. Returns false if either the
/// stack allocation or the task creation failed; in that case the caller
/// stays responsible for any other resources it allocated for the task.
inline bool start_pinned_task(TaskFunction_t fn, const char *name, uint32_t stack_bytes, void *param, UBaseType_t prio,
                              BaseType_t core, bool psram_stack, const char *log_tag, TaskHandle_t *handle_out,
                              StaticTask_t *tcb_out, StackType_t **stack_out) {
  *handle_out = nullptr;
  const uint32_t stack_words = (stack_bytes + sizeof(StackType_t) - 1) / sizeof(StackType_t);
  if (psram_stack) {
    *stack_out = nullptr;
    RAMAllocator<StackType_t> alloc(RAMAllocator<StackType_t>::ALLOC_EXTERNAL);
    *stack_out = alloc.allocate(stack_words);
    if (*stack_out == nullptr) {
      log_error(log_tag, "Failed to allocate PSRAM stack for %s task", name);
      return false;
    }
    *handle_out = xTaskCreateStaticPinnedToCore(fn, name, stack_bytes, param, prio, *stack_out, tcb_out, core);
  } else {
    BaseType_t ok = xTaskCreatePinnedToCore(fn, name, stack_bytes, param, prio, handle_out, core);
    if (ok != pdPASS) {
      *handle_out = nullptr;
    }
  }
  if (*handle_out == nullptr) {
    log_error(log_tag, "Failed to create %s task", name);
    if (psram_stack && *stack_out != nullptr) {
      RAMAllocator<StackType_t> alloc(RAMAllocator<StackType_t>::ALLOC_EXTERNAL);
      alloc.deallocate(*stack_out, stack_words);
      *stack_out = nullptr;
    }
    return false;
  }
  return true;
}

}  // namespace esphome::esp_audio_stack

#endif  // USE_ESP32
