#include "esphome/core/static_task.h"

#ifdef USE_ESP32

#include "esphome/core/helpers.h"

namespace esphome {

bool StaticTask::create(TaskFunction_t fn, const char *name, uint32_t stack_size, void *param, UBaseType_t priority,
                        bool use_psram) {
  if (this->handle_ != nullptr) {
    // Task is already created; must call destroy() first
    return false;
  }

  if (this->stack_buffer_ != nullptr && (stack_size > this->stack_size_ || use_psram != this->use_psram_)) {
    // Existing buffer is too small or wrong memory type; deallocate to reallocate below
    RAMAllocator<StackType_t> allocator(this->use_psram_ ? RAMAllocator<StackType_t>::ALLOC_EXTERNAL
                                                         : RAMAllocator<StackType_t>::ALLOC_INTERNAL);
    allocator.deallocate(this->stack_buffer_, this->stack_size_);
    this->stack_buffer_ = nullptr;
  }

  if (this->stack_buffer_ == nullptr) {
    this->stack_size_ = stack_size;
    this->use_psram_ = use_psram;
    RAMAllocator<StackType_t> allocator(use_psram ? RAMAllocator<StackType_t>::ALLOC_EXTERNAL
                                                  : RAMAllocator<StackType_t>::ALLOC_INTERNAL);
    this->stack_buffer_ = allocator.allocate(stack_size);
  }
  if (this->stack_buffer_ == nullptr) {
    return false;
  }

  this->handle_ = xTaskCreateStatic(fn, name, this->stack_size_, param, priority, this->stack_buffer_, &this->tcb_);
  if (this->handle_ == nullptr) {
    this->deallocate();
    return false;
  }
  return true;
}

bool StaticTask::destroy() {
  if (this->handle_ == nullptr) {
    return true;
  }

  // Suspending takes the task off the ready and event lists, so nothing can schedule it again. It only asks
  // the other core to yield though, so the task may still be running on it for a moment.
  vTaskSuspend(this->handle_);
  if (eTaskGetState(this->handle_) != eSuspended) {
    // The task is still running on the other core and using its stack. Deleting it now would only put it on
    // the termination list and return, so the caller has to try again once it has been swapped out.
    return false;
  }

  // The task cannot run again, so the delete completes right away instead of being left to the idle task.
  TaskHandle_t handle = this->handle_;
  this->handle_ = nullptr;
  vTaskDelete(handle);
  return true;
}

bool StaticTask::deallocate() {
  if (!this->destroy()) {
    return false;
  }
  if (this->stack_buffer_ != nullptr) {
    RAMAllocator<StackType_t> allocator(this->use_psram_ ? RAMAllocator<StackType_t>::ALLOC_EXTERNAL
                                                         : RAMAllocator<StackType_t>::ALLOC_INTERNAL);
    allocator.deallocate(this->stack_buffer_, this->stack_size_);
    this->stack_buffer_ = nullptr;
    this->stack_size_ = 0;
  }
  return true;
}

}  // namespace esphome

#endif  // USE_ESP32
