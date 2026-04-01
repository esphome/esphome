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
 * FreeRTOS critical sections.
 *
 * @tparam T The type of elements stored in the queue (stored as pointers)
 * @tparam SIZE The maximum number of elements
 */

namespace esphome {

template<class T, uint8_t SIZE> class FreeRTOSQueue {
 public:
  FreeRTOSQueue() : dropped_count_(0) { this->handle_ = xQueueCreate(SIZE, sizeof(T *)); }

  bool push(T *element) {
    if (element == nullptr || this->handle_ == nullptr)
      return false;

    if (xQueueSend(this->handle_, &element, 0) != pdPASS) {
      this->dropped_count_++;
      return false;
    }
    return true;
  }

  T *pop() {
    if (this->handle_ == nullptr)
      return nullptr;

    T *element;
    if (xQueueReceive(this->handle_, &element, 0) != pdTRUE) {
      return nullptr;
    }
    return element;
  }

  uint16_t get_and_reset_dropped_count() {
    uint16_t count = this->dropped_count_;
    if (count == 0)
      return 0;
    this->dropped_count_ = 0;
    return count;
  }

  void increment_dropped_count() { this->dropped_count_++; }

  bool empty() const {
    if (this->handle_ == nullptr)
      return true;
    return uxQueueMessagesWaiting(this->handle_) == 0;
  }

  bool full() const {
    if (this->handle_ == nullptr)
      return true;
    return uxQueueSpacesAvailable(this->handle_) == 0;
  }

  size_t size() const {
    if (this->handle_ == nullptr)
      return 0;
    return uxQueueMessagesWaiting(this->handle_);
  }

 protected:
  QueueHandle_t handle_;
  volatile uint16_t dropped_count_;
};

}  // namespace esphome

#endif  // ESPHOME_THREAD_MULTI_NO_ATOMICS
