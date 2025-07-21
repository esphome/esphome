#pragma once

#if defined(USE_ESP32)

#include <atomic>
#include <cstddef>
#include "esphome/core/helpers.h"
#include "esphome/core/lock_free_queue.h"

namespace esphome {

// Event Pool - On-demand pool of objects to avoid heap fragmentation
// Events are allocated on first use and reused thereafter, growing to peak usage
// @tparam T The type of objects managed by the pool (must have a release() method)
// @tparam SIZE The maximum number of objects in the pool (1-255, limited by uint8_t)
template<class T, uint8_t SIZE> class EventPool {
 public:
  EventPool() : total_created_(0) {}

  ~EventPool() {
    // Clean up any remaining events in the free list
    // IMPORTANT: This destructor assumes no concurrent access. The EventPool must not
    // be destroyed while any thread might still call allocate() or release().
    // In practice, this is typically ensured by destroying the pool only during
    // component shutdown when all producer/consumer threads have been stopped.
    T *event;
    RAMAllocator<T> allocator(RAMAllocator<T>::ALLOC_INTERNAL);
    while ((event = this->free_list_.pop()) != nullptr) {
      // Call destructor
      event->~T();
      // Deallocate using RAMAllocator
      allocator.deallocate(event, 1);
    }
  }

  // Allocate an event from the pool
  // Returns nullptr if pool is full
  T *allocate() {
    // Try to get from free list first
    T *event = this->free_list_.pop();
    if (event != nullptr)
      return event;

    // Need to create a new event
    if (this->total_created_ >= SIZE) {
      // Pool is at capacity
      return nullptr;
    }

    // Use internal RAM for better performance
    RAMAllocator<T> allocator(RAMAllocator<T>::ALLOC_INTERNAL);
    event = allocator.allocate(1);

    if (event == nullptr) {
      // Memory allocation failed
      return nullptr;
    }

    // Placement new to construct the object
    new (event) T();
    this->total_created_++;
    return event;
  }

  // Return an event to the pool for reuse
  void release(T *event) {
    if (event != nullptr) {
      // Clean up the event's allocated memory
      event->release();
      this->free_list_.push(event);
    }
  }

 private:
  LockFreeQueue<T, SIZE> free_list_;  // Free events ready for reuse
  uint8_t total_created_;             // Total events created (high water mark, max 255)
};

}  // namespace esphome

#endif  // defined(USE_ESP32)
