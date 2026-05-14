#pragma once

#ifdef USE_ESP8266

// Mirrors esphome::EventPool but uses ESP8266Queue instead of LockFreeQueue
// for its internal free list, avoiding the sub-word atomic operations that
// require libatomic (unavailable in the PlatformIO xtensa toolchain).
//
// See esp8266_queue.h for the rationale on interrupt masking vs atomics.

#include "esp8266_queue.h"
#include "esphome/core/helpers.h"

namespace esphome::espnow {

template<class T, uint8_t SIZE> class ESP8266EventPool {
 public:
  ESP8266EventPool() : total_created_(0) {}

  ~ESP8266EventPool() {
    T *event;
    RAMAllocator<T> allocator(RAMAllocator<T>::ALLOC_INTERNAL);
    while ((event = this->free_list_.pop()) != nullptr) {
      event->~T();
      allocator.deallocate(event, 1);
    }
  }

  T *allocate() {
    T *event = this->free_list_.pop();
    if (event != nullptr)
      return event;

    if (this->total_created_ >= SIZE)
      return nullptr;

    RAMAllocator<T> allocator(RAMAllocator<T>::ALLOC_INTERNAL);
    event = allocator.allocate(1);
    if (event == nullptr)
      return nullptr;

    new (event) T();
    this->total_created_++;
    return event;
  }

  void release(T *event) {
    if (event != nullptr) {
      event->release();
      this->free_list_.push(event);
    }
  }

 private:
  ESP8266Queue<T, SIZE> free_list_;
  uint8_t total_created_;
};

}  // namespace esphome::espnow

#endif  // USE_ESP8266
