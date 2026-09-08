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
  /// @param stack_size Stack size in bytes (StackType_t is a byte on ESP-IDF)
  /// @param param      Parameter passed to task function
  /// @param priority   FreeRTOS task priority
  /// @param use_psram  If true, allocate stack in PSRAM; otherwise internal RAM
  /// @return true on success
  bool create(TaskFunction_t fn, const char *name, uint32_t stack_size, void *param, UBaseType_t priority,
              bool use_psram);

  /// @brief Delete the task, keeping the stack buffer allocated for reuse by a subsequent create() call.
  /// The task must have finished its work and parked itself, either suspended or blocked indefinitely: it is
  /// suspended here so that it cannot be scheduled again, and it is given no chance to clean up.
  /// @return true if the task was deleted; false if it is still running on another core, in which case the
  /// caller should try again later.
  bool destroy();

  /// @brief Delete the task (if created) and free the stack buffer.
  /// @return true if the stack buffer was freed; false if the task is still running on another core, in
  /// which case the caller should try again later.
  bool deallocate();

 protected:
  TaskHandle_t handle_{nullptr};
  StaticTask_t tcb_;
  StackType_t *stack_buffer_{nullptr};
  uint32_t stack_size_{0};
  bool use_psram_{false};
};

}  // namespace esphome

#endif  // USE_ESP32
