#pragma once

#ifdef USE_ESP32

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cstdint>

namespace esphome {

/** Helper for FreeRTOS static task management.
 * Bundles TaskHandle_t, StaticTask_t, and the stack buffer into one object with create/destroy methods.
 */
class StaticTask {
 public:
  /// @brief Check if the task has been created and not yet destroyed.
  bool is_created() const { return this->handle_ != nullptr; }

  /// @brief Get the FreeRTOS task handle.
  TaskHandle_t get_handle() const { return this->handle_; }

  /// @brief Allocate stack and create task.
  /// @param fn         Task function
  /// @param name       Task name (for debug)
  /// @param stack_size Stack size in StackType_t words
  /// @param param      Parameter passed to task function
  /// @param priority   FreeRTOS task priority
  /// @param use_psram  If true, allocate stack in PSRAM; otherwise internal RAM
  /// @return true on success
  bool create(TaskFunction_t fn, const char *name, uint32_t stack_size, void *param, UBaseType_t priority,
              bool use_psram);

  /// @brief Delete the task but keep the stack buffer allocated for reuse by a subsequent create() call.
  void destroy();

  /// @brief Delete the task (if running) and free the stack buffer.
  void deallocate();

 protected:
  TaskHandle_t handle_{nullptr};
  StaticTask_t tcb_;
  StackType_t *stack_buffer_{nullptr};
  uint32_t stack_size_{0};
  bool use_psram_{false};
};

}  // namespace esphome

#endif  // USE_ESP32
