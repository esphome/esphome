#pragma once

#if defined(USE_ESP32) || defined(USE_LIBRETINY)

#include <atomic>
#include <cstddef>

#if defined(USE_ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#elif defined(USE_LIBRETINY)
#include <FreeRTOS.h>
#include <task.h>
#endif

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

template<class T, uint8_t SIZE> class LockFreeQueue {
 public:
  LockFreeQueue() : head_(0), tail_(0), dropped_count_(0), task_to_notify_(nullptr) {}

  bool push(T *element) {
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

    // Check if queue was empty before push
    bool was_empty = (current_tail == head_before);

    buffer_[current_tail] = element;
    tail_.store(next_tail, std::memory_order_release);

    // Notify optimization: only notify if we need to
    if (task_to_notify_ != nullptr) {
      if (was_empty) {
        // Queue was empty - consumer might be going to sleep, must notify
        xTaskNotifyGive(task_to_notify_);
      } else {
        // Queue wasn't empty - check if consumer has caught up to previous tail
        uint8_t head_after = head_.load(std::memory_order_acquire);
        if (head_after == current_tail) {
          // Consumer just caught up to where tail was - might go to sleep, must notify
          // Note: There's a benign race here - between reading head_after and calling
          // xTaskNotifyGive(), the consumer could advance further. This would result
          // in an unnecessary wake-up, but is harmless and extremely rare in practice.
          xTaskNotifyGive(task_to_notify_);
        }
        // Otherwise: consumer is still behind, no need to notify
      }
    }

    return true;
  }

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

  // Set the FreeRTOS task handle to notify when items are pushed to the queue
  // This enables efficient wake-up of a consumer task that's waiting for data
  // @param task The FreeRTOS task handle to notify, or nullptr to disable notifications
  void set_task_to_notify(TaskHandle_t task) { task_to_notify_ = task; }

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
  // Task handle for notification (optional)
  TaskHandle_t task_to_notify_;
};

}  // namespace esphome

#endif  // defined(USE_ESP32) || defined(USE_LIBRETINY)
