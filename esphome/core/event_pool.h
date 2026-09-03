#pragma once

#if defined(USE_ESP32) || defined(USE_ZEPHYR) || defined(USE_LIBRETINY) || defined(USE_RP2) || defined(USE_HOST)

#include <atomic>
#include <cstddef>
#include "esphome/core/helpers.h"
#include "esphome/core/lock_free_queue.h"

namespace esphome {

// Event Pool - On-demand pool of objects to avoid heap fragmentation
// Events are allocated on first use and reused thereafter, growing to peak
// usage; warm() pre-creates every entry up front for malloc-free producers
// @tparam T The type of objects managed by the pool (must have a release() method)
// @tparam SIZE The maximum number of objects in the pool (1-254, limited by uint8_t and the +1 free-list slot)
//
// SIZING: When paired with a LockFreeQueue<T, Q_SIZE>, the pool SIZE should be
// Q_SIZE - 1 (the queue's actual capacity, since the ring buffer reserves one slot).
// This ensures allocate() returns nullptr before push() can fail, which:
//  - Prevents the allocate-succeeds-but-push-fails mismatch that permanently
//    leaks a pool slot (the element is never returned to the pool)
//  - Avoids needing release() on the producer path after a failed push(),
//    preserving the SPSC contract on the internal free list
template<class T, uint8_t SIZE> class EventPool {
  // The free list ring must hold all SIZE objects at once (a fully drained
  // pool), and LockFreeQueue reserves one slot — so it is sized SIZE + 1,
  // which caps SIZE at 254.
  static_assert(SIZE < 255, "EventPool SIZE must be at most 254");

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
    return this->create_();
  }

  // Return an event to the pool for reuse
  void release(T *event) {
    if (event != nullptr) {
      // Clean up the event's allocated memory
      event->release();
      this->free_list_.push(event);
    }
  }

  // Pre-create every pool entry so allocate() is always a free-list pop
  // (for producers that must never malloc, e.g. IRQ-context handlers).
  // Call from setup(); on false the heap could not supply every entry and
  // the caller should mark_failed() — an incomplete warm puts malloc()
  // back on the producer path. Tops the pool up from any quiescent state
  // (entries that already exist are counted, not re-created); must not run
  // concurrently with allocate()/release().
  bool warm() {
    // NOLINTNEXTLINE(clang-analyzer-unix.Malloc) -- ownership transfers to the free list
    while (this->total_created_ < SIZE) {
      T *event = this->create_();
      if (event == nullptr)
        return false;
      this->free_list_.push(event);
    }
    return true;
  }

 private:
  // Create and count one new object (shared by allocate() and warm()).
  // Returns nullptr at capacity or when the heap is exhausted.
  T *create_() {
    if (this->total_created_ >= SIZE) {
      // Pool is at capacity
      return nullptr;
    }
    // Use internal RAM for better performance
    RAMAllocator<T> allocator(RAMAllocator<T>::ALLOC_INTERNAL);
    T *event = allocator.allocate(1);
    if (event == nullptr) {
      // Memory allocation failed
      return nullptr;
    }
    // Placement new to construct the object
    new (event) T();
    this->total_created_++;
    return event;
  }

  // SIZE + 1 slots so all SIZE objects fit when the pool is fully drained
  // (the ring reserves one slot); otherwise the last release() of a
  // completely returned pool would drop, permanently orphaning one object.
  LockFreeQueue<T, static_cast<uint8_t>(SIZE + 1)> free_list_;  // Free events ready for reuse
  uint8_t total_created_;                                       // Total events created (high water mark, max 254)
};

}  // namespace esphome

#endif  // defined(USE_ESP32) || defined(USE_ZEPHYR) || defined(USE_LIBRETINY) || defined(USE_RP2) || defined(USE_HOST)
