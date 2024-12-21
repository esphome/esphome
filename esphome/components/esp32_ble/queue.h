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

 protected:
  std::queue<T *> q_;
  SemaphoreHandle_t m_;
};

}  // namespace esp32_ble
}  // namespace esphome

#endif
