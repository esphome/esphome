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

void StaticTask::destroy() {
  if (this->handle_ != nullptr) {
    TaskHandle_t handle = this->handle_;
    this->handle_ = nullptr;
    vTaskDelete(handle);
  }
}

void StaticTask::deallocate() {
  this->destroy();
  if (this->stack_buffer_ != nullptr) {
    RAMAllocator<StackType_t> allocator(this->use_psram_ ? RAMAllocator<StackType_t>::ALLOC_EXTERNAL
                                                         : RAMAllocator<StackType_t>::ALLOC_INTERNAL);
    allocator.deallocate(this->stack_buffer_, this->stack_size_);
    this->stack_buffer_ = nullptr;
    this->stack_size_ = 0;
  }
}

}  // namespace esphome

#endif  // USE_ESP32
