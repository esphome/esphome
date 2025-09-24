#pragma once

#ifdef USE_ESP32

#include <atomic>
#include <cstddef>

/*
 * BLE events come in from a separate Task (thread) in the ESP32 stack. Rather
 * than using mutex-based locking, this lock-free queue allows the BLE
 * task to enqueue events without blocking. The main loop() then processes
 * these events at a safer time.
 *
 * This is a Single-Producer Single-Consumer (SPSC) lock-free ring buffer.
 * The BLE task is the only producer, and the main loop() is the only consumer.
 */

namespace esphome {
namespace esp32_ble {

template<class T, size_t SIZE> class LockFreeQueue {
 public:
  LockFreeQueue() : head_(0), tail_(0), dropped_count_(0) {}

  bool push(T *element) {
    if (element == nullptr)
      return false;

    size_t current_tail = tail_.load(std::memory_order_relaxed);
    size_t next_tail = (current_tail + 1) % SIZE;

    if (next_tail == head_.load(std::memory_order_acquire)) {
      // Buffer full
      dropped_count_.fetch_add(1, std::memory_order_relaxed);
      return false;
    }

    buffer_[current_tail] = element;
    tail_.store(next_tail, std::memory_order_release);
    return true;
  }

  T *pop() {
    size_t current_head = head_.load(std::memory_order_relaxed);

    if (current_head == tail_.load(std::memory_order_acquire)) {
      return nullptr;  // Empty
    }

    T *element = buffer_[current_head];
    head_.store((current_head + 1) % SIZE, std::memory_order_release);
    return element;
  }

  size_t size() const {
    size_t tail = tail_.load(std::memory_order_acquire);
    size_t head = head_.load(std::memory_order_acquire);
    return (tail - head + SIZE) % SIZE;
  }

  size_t get_and_reset_dropped_count() { return dropped_count_.exchange(0, std::memory_order_relaxed); }

  void increment_dropped_count() { dropped_count_.fetch_add(1, std::memory_order_relaxed); }

  bool empty() const { return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire); }

  bool full() const {
    size_t next_tail = (tail_.load(std::memory_order_relaxed) + 1) % SIZE;
    return next_tail == head_.load(std::memory_order_acquire);
  }

 protected:
  T *buffer_[SIZE];
  std::atomic<size_t> head_;
  std::atomic<size_t> tail_;
  std::atomic<size_t> dropped_count_;
};

}  // namespace esp32_ble
}  // namespace esphome

#endif
