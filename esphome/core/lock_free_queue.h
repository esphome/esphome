#pragma once

#if defined(USE_ESP32)

#include <atomic>
#include <cstddef>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

/*
 * Lock-free queue for single-producer single-consumer scenarios.
 * This allows one thread to push items and another to pop them without
 * blocking each other.
 *
 * This is a Single-Producer Single-Consumer (SPSC) lock-free ring buffer.
 * Available on platforms with FreeRTOS support (ESP32, LibreTiny).
 *
 * Common use cases:
 * - BLE events: BLE task produces, main loop consumes
 * - MQTT messages: main task produces, MQTT thread consumes
 *
 * @tparam T The type of elements stored in the queue (must be a pointer type)
 * @tparam SIZE The maximum number of elements (1-255, limited by uint8_t indices)
 */

namespace esphome {

// Base lock-free queue without task notification
template<class T, uint8_t SIZE> class LockFreeQueue {
 public:
  LockFreeQueue() : dropped_count_(0), head_(0), tail_(0) {}

  bool push(T *element) {
    bool was_empty;
    uint8_t old_tail;
    return push_internal_(element, was_empty, old_tail);
  }

 protected:
  // Internal push that reports queue state - for use by derived classes
  bool push_internal_(T *element, bool &was_empty, uint8_t &old_tail) {
    if (element == nullptr)
      return false;

    uint8_t current_tail = tail_.load(std::memory_order_relaxed);
    uint8_t next_tail = (current_tail + 1) % SIZE;

    // Read head before incrementing tail
    uint8_t head_before = head_.load(std::memory_order_acquire);

    if (next_tail == head_before) {
      // Buffer full
      dropped_count_.fetch_add(1, std::memory_order_relaxed);
      return false;
    }

    was_empty = (current_tail == head_before);
    old_tail = current_tail;

    buffer_[current_tail] = element;
    tail_.store(next_tail, std::memory_order_release);

    return true;
  }

 public:
  T *pop() {
    uint8_t current_head = head_.load(std::memory_order_relaxed);

    if (current_head == tail_.load(std::memory_order_acquire)) {
      return nullptr;  // Empty
    }

    T *element = buffer_[current_head];
    head_.store((current_head + 1) % SIZE, std::memory_order_release);
    return element;
  }

  size_t size() const {
    uint8_t tail = tail_.load(std::memory_order_acquire);
    uint8_t head = head_.load(std::memory_order_acquire);
    return (tail - head + SIZE) % SIZE;
  }

  uint16_t get_and_reset_dropped_count() { return dropped_count_.exchange(0, std::memory_order_relaxed); }

  void increment_dropped_count() { dropped_count_.fetch_add(1, std::memory_order_relaxed); }

  bool empty() const { return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire); }

  bool full() const {
    uint8_t next_tail = (tail_.load(std::memory_order_relaxed) + 1) % SIZE;
    return next_tail == head_.load(std::memory_order_acquire);
  }

 protected:
  T *buffer_[SIZE];
  // Atomic: written by producer (push/increment), read+reset by consumer (get_and_reset)
  std::atomic<uint16_t> dropped_count_;  // 65535 max - more than enough for drop tracking
  // Atomic: written by consumer (pop), read by producer (push) to check if full
  // Using uint8_t limits queue size to 255 elements but saves memory and ensures
  // atomic operations are efficient on all platforms
  std::atomic<uint8_t> head_;
  // Atomic: written by producer (push), read by consumer (pop) to check if empty
  std::atomic<uint8_t> tail_;
};

// Extended queue with task notification support
template<class T, uint8_t SIZE> class NotifyingLockFreeQueue : public LockFreeQueue<T, SIZE> {
 public:
  NotifyingLockFreeQueue() : LockFreeQueue<T, SIZE>(), task_to_notify_(nullptr) {}

  bool push(T *element) {
    bool was_empty;
    uint8_t old_tail;
    bool result = this->push_internal_(element, was_empty, old_tail);

    // Notify optimization: only notify if we need to
    if (result && task_to_notify_ != nullptr &&
        (was_empty || this->head_.load(std::memory_order_acquire) == old_tail)) {
      // Notify in two cases:
      // 1. Queue was empty - consumer might be going to sleep
      // 2. Consumer just caught up to where tail was - might go to sleep
      // Note: There's a benign race in case 2 - between reading head and calling
      // xTaskNotifyGive(), the consumer could advance further. This would result
      // in an unnecessary wake-up, but is harmless and extremely rare in practice.
      xTaskNotifyGive(task_to_notify_);
    }
    // Otherwise: consumer is still behind, no need to notify

    return result;
  }

  // Set the FreeRTOS task handle to notify when items are pushed to the queue
  // This enables efficient wake-up of a consumer task that's waiting for data
  // @param task The FreeRTOS task handle to notify, or nullptr to disable notifications
  void set_task_to_notify(TaskHandle_t task) { task_to_notify_ = task; }

 private:
  TaskHandle_t task_to_notify_;
};

}  // namespace esphome

#endif  // defined(USE_ESP32)
