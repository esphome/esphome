#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <functional>
#include <atomic>

namespace esphome::camera {

/// A task that executes jobs asynchronously, without blocking other components.
/// Designed for running long operations (e.g. camera processors or encoders)
class Task {
 public:
  using JobFunction = std::function<void()>;
  /// Creates a Task with custom settings.
  /// @param core CPU core to run the task on.
  /// @param stack_size Stack size for task.
  /// @param priority Task priority.
  /// @param delay Delay in ms betwwen work cycles.
  Task(uint8_t core, uint32_t stack_size, UBaseType_t priority, uint8_t delay);

  /// Initializes and starts the task.
  /// @return true if the task was successfully initialized.
  bool init();

  /// Starts a new asynchronous job if none is currently running.
  /// @param job Function or lambda to execute in the background.
  /// @return true if the job was accepted, false if one is already running.
  bool start(JobFunction job);

  /// @return true if a job is running, false otherwise.
  bool is_running() const { return has_job_.load(); }

  /// Logs the task's configuration.
  void log_config(const char *tag);

 private:
  static void task_entry_point(void *param);
  void task_loop_();

  uint8_t core_{};
  uint8_t delay_{};
  UBaseType_t priority_{};
  uint32_t stack_size_{};
  TaskHandle_t task_handle_{};
  std::atomic<bool> has_job_{};
  JobFunction job_;
};

}  // namespace esphome::camera
