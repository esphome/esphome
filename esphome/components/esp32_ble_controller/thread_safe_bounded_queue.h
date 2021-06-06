#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace esphome {
namespace esp32_ble_controller {

/**
 * Thread-safe non-blocking bounded queue to pass values between Free RTOS tasks.
 */
template <typename T>
class ThreadSafeBoundedQueue {
public:
  /// Creates a bounded queue with the given size (i.e. maximum number of objects that can be queued).
  ThreadSafeBoundedQueue(unsigned int size);

  /**
   * Pushes the given object into the queue, the queue takes over ownership.
   * @param object object to append to the queue (treated as r-value)
   * @return true if successful, false if queue is full
   */
  boolean push(T&& object);

  /**
   * Takes the first queued element from the queue (if any) and moves it to the given object.
   * @param object object to store the dequeued value
   * @return true if successful, false if queue was empty
   */
  boolean take(T& object);

private:
  QueueHandle_t queue;
};

template <typename T>
ThreadSafeBoundedQueue<T>::ThreadSafeBoundedQueue(unsigned int size) {
  queue = xQueueCreate( size, sizeof( void* ) );
}

template <typename T>
boolean ThreadSafeBoundedQueue<T>::push(T&& object) {
  T* pointer_to_copy = new T();
  *pointer_to_copy = std::move(object);

  // add the pointer to the queue, not the object itself
  auto result = xQueueSend(queue, &pointer_to_copy, 20L / portTICK_PERIOD_MS);
  return result == pdPASS;
}

template <typename T>
boolean ThreadSafeBoundedQueue<T>::take(T& object) {
  T* pointer_to_object; 
  auto result = xQueueReceive(queue, &pointer_to_object, 0);
  if (result != pdPASS) {
    return false;
  }

  object = std::move(*pointer_to_object);
  delete pointer_to_object;
  return true;
}

} // namespace esp32_ble_controller
} // namespace esphome
