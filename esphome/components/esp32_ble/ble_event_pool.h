#pragma once

#ifdef USE_ESP32

#include <atomic>
#include <cstddef>
#include "ble_event.h"
#include "queue.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace esp32_ble {

// BLE Event Pool - On-demand pool of BLEEvent objects to avoid heap fragmentation
// Events are allocated on first use and reused thereafter, growing to peak usage
template<uint8_t SIZE> class BLEEventPool {
 public:
  BLEEventPool() : total_created_(0) {}

  ~BLEEventPool() {
    // Clean up any remaining events in the free list
    BLEEvent *event;
    while ((event = this->free_list_.pop()) != nullptr) {
      delete event;
    }
  }

  // Allocate an event from the pool
  // Returns nullptr if pool is full
  BLEEvent *allocate() {
    // Try to get from free list first
    BLEEvent *event = this->free_list_.pop();
    if (event != nullptr)
      return event;

    // Need to create a new event
    if (this->total_created_ >= SIZE) {
      // Pool is at capacity
      return nullptr;
    }

    // Use internal RAM for better performance
    RAMAllocator<BLEEvent> allocator(RAMAllocator<BLEEvent>::ALLOC_INTERNAL);
    event = allocator.allocate(1);

    if (event == nullptr) {
      // Memory allocation failed
      return nullptr;
    }

    // Placement new to construct the object
    new (event) BLEEvent();
    this->total_created_++;
    return event;
  }

  // Return an event to the pool for reuse
  void release(BLEEvent *event) {
    if (event != nullptr) {
      this->free_list_.push(event);
    }
  }

 private:
  LockFreeQueue<BLEEvent, SIZE> free_list_;  // Free events ready for reuse
  uint8_t total_created_;                    // Total events created (high water mark)
};

}  // namespace esp32_ble
}  // namespace esphome

#endif
