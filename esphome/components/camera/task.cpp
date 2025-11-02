#include "task.h"
#include "esphome/core/log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <utility>

namespace esphome::camera {

Task::Task(uint8_t core, uint32_t stack_size, UBaseType_t priority, uint8_t delay)
    : core_(core), delay_(delay), priority_(priority), stack_size_(stack_size) {}

bool Task::init() {
  if (task_handle_)
    return false;

  BaseType_t error = xTaskCreatePinnedToCore(task_entry_point, "job", this->stack_size_, this, this->priority_,
                                             &task_handle_, this->core_);

  return error == pdPASS;
}

bool Task::start(JobFunction job) {
  if (has_job_.load())
    return false;

  job_ = std::move(job);
  has_job_.store(true);
  return true;
}

void Task::task_entry_point(void *param) { static_cast<Task *>(param)->task_loop_(); }

void Task::log_config(const char *tag) {
  ESP_LOGCONFIG(tag,
                "  Task:\n"
                "    Core: %u\n"
                "    Stack Size: %u\n"
                "    Priority: %u\n"
                "    Delay: %u\n",
                this->core_, this->stack_size_, this->priority_, this->delay_);
}
void Task::task_loop_() {
  for (;;) {
    if (has_job_.load()) {
      job_();
      has_job_.store(false);
    }

    vTaskDelay(pdMS_TO_TICKS(this->delay_));
  }
}

}  // namespace esphome::camera
