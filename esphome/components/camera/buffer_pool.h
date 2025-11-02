#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <functional>

namespace esphome::camera {

/// Simple pool for reusable buffers.
/// A factory function is used to create buffers, so T does not need a specific constructor.
/// @param T Buffer type can be any subclass of camera::Buffer.
template<typename T> class BufferPool {
 public:
  using Factory = std::function<T *()>;

  /// Initializes the pool with a number of buffers.
  /// @param count Number of buffers to create.
  /// @param factory Factory function to create a new buffer of type T.
  /// @return True if all buffers were allocated successfully, false otherwise.
  bool init(size_t count, Factory factory) {
    if (this->initialized_)
      return false;

    this->initialized_ = true;
    this->handle_ = xQueueCreate(count, sizeof(T *));

    for (size_t i = 0; i < count; ++i) {
      T *buffer = factory();
      if (!buffer)
        return false;

      if (xQueueSend(handle_, &buffer, 0) != pdPASS)
        return false;
    }

    return true;
  }
  /// @return a buffer from the pool.
  T *acquire() {
    T *buffer = nullptr;
    xQueueReceive(handle_, &buffer, 0);
    return buffer;
  }
  /// Return a buffer to the pool.
  void release(T *buffer) { xQueueSend(handle_, &buffer, 0); }

 protected:
  bool initialized_{};
  QueueHandle_t handle_;
};

}  // namespace esphome::camera
