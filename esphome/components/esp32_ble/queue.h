#pragma once

#ifdef USE_ESP32

#include <mutex>
#include <queue>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

/*
 * BLE events come in from a separate Task (thread) in the ESP32 stack. Rather
 * than trying to deal with various locking strategies, all incoming GAP and GATT
 * events will simply be placed on a semaphore guarded queue. The next time the
 * component runs loop(), these events are popped off the queue and handed at
 * this safer time.
 */

namespace esphome {
namespace esp32_ble {

template<class T> class Queue {
 public:
  Queue() { m_ = xSemaphoreCreateMutex(); }

  void push(T *element) {
    if (element == nullptr)
      return;
    // It is not called from main loop. Thus it won't block main thread.
    xSemaphoreTake(m_, portMAX_DELAY);
    q_.push(element);
    xSemaphoreGive(m_);
  }

  T *pop() {
    T *element = nullptr;

    if (xSemaphoreTake(m_, 5L / portTICK_PERIOD_MS)) {
      if (!q_.empty()) {
        element = q_.front();
        q_.pop();
      }
      xSemaphoreGive(m_);
    }
    return element;
  }

  size_t size() const {
    // Lock-free size check. While std::queue::size() is not thread-safe, we intentionally
    // avoid locking here to prevent blocking the BLE callback thread. The size is only
    // used to decide whether to drop incoming events when the queue is near capacity.
    // With a queue limit of 40-64 events and normal processing, dropping events should
    // be extremely rare. When it does approach capacity, being off by 1-2 events is
    // acceptable to avoid blocking the BLE stack's time-sensitive callbacks.
    // Trade-off: We prefer occasional dropped events over potential BLE stack delays.
    return q_.size();
  }

 protected:
  std::queue<T *> q_;
  SemaphoreHandle_t m_;
};

}  // namespace esp32_ble
}  // namespace esphome

#endif
