#pragma once

#include "esphome/core/defines.h"

#ifdef ESPHOME_THREAD_MULTI_NO_ATOMICS

#include <cstddef>
#include <cstdint>

#include <FreeRTOS.h>
#include <queue.h>

/*
 * FreeRTOS queue wrapper for single-producer single-consumer scenarios on
 * platforms without hardware atomic support (e.g. BK72xx ARM968E-S).
 *
 * Provides the same API as LockFreeQueue (push, pop, get_and_reset_dropped_count,
 * empty, full, size) but uses xQueue internally, which synchronizes via
 * FreeRTOS critical sections. Uses xQueueCreateStatic so the queue storage
 * lives in BSS with zero runtime heap allocation.
 *
 * @tparam T The type of elements stored in the queue (stored as pointers)
 * @tparam SIZE The maximum number of elements
 */

namespace esphome {

template<class T, uint8_t SIZE> class FreeRTOSQueue {
 public:
  FreeRTOSQueue() : dropped_count_(0) {
    this->handle_ = xQueueCreateStatic(SIZE, sizeof(T *), this->storage_, &this->queue_buf_);
  }

  // No destructor — ESPHome components are never destroyed. Intentionally
  // omitted to avoid pulling in vQueueDelete code on resource-constrained targets.

  // Non-copyable, non-movable — queue handle is not transferable
  FreeRTOSQueue(const FreeRTOSQueue &) = delete;
  FreeRTOSQueue &operator=(const FreeRTOSQueue &) = delete;
  FreeRTOSQueue(FreeRTOSQueue &&) = delete;
  FreeRTOSQueue &operator=(FreeRTOSQueue &&) = delete;

  bool push(T *element) {
    if (element == nullptr)
      return false;

    if (xQueueSend(this->handle_, &element, 0) != pdPASS) {
      this->increment_dropped_count();
      return false;
    }
    return true;
  }

  T *pop() {
    T *element;
    if (xQueueReceive(this->handle_, &element, 0) != pdTRUE) {
      return nullptr;
    }
    return element;
  }

  uint16_t get_and_reset_dropped_count() {
    // Fast path: plain read of aligned uint16_t is a single ARM load instruction.
    // Worst case is reading a stale zero and reporting drops one iteration later.
    // Avoids critical section overhead on every loop() call since drops are rare.
    if (this->dropped_count_ == 0)
      return 0;
    // Declare outside critical section — BK72xx portENTER_CRITICAL may introduce a scope
    uint16_t count;
    portENTER_CRITICAL();
    count = this->dropped_count_;
    this->dropped_count_ = 0;
    portEXIT_CRITICAL();
    return count;
  }

  void increment_dropped_count() {
    portENTER_CRITICAL();
    this->dropped_count_++;
    portEXIT_CRITICAL();
  }

  bool empty() const { return uxQueueMessagesWaiting(this->handle_) == 0; }

  bool full() const { return uxQueueSpacesAvailable(this->handle_) == 0; }

  size_t size() const { return uxQueueMessagesWaiting(this->handle_); }

 protected:
  // Static storage for the queue — lives in BSS, no heap allocation
  uint8_t storage_[SIZE * sizeof(T *)];
  StaticQueue_t queue_buf_;
  QueueHandle_t handle_;
  uint16_t dropped_count_;
};

}  // namespace esphome

#endif  // ESPHOME_THREAD_MULTI_NO_ATOMICS
