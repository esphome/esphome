#pragma once

#include <functional>
#include <queue>

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
    for (size_t i = 0; i < count; ++i) {
      T *buffer = factory();
      if (!buffer)
        return false;

      this->free_buffers_.push(buffer);
    }

    return true;
  }
  /// @return a buffer from the pool.
  T *acquire() {
    if (this->free_buffers_.empty())
      return nullptr;

    T *buffer = this->free_buffers_.front();
    this->free_buffers_.pop();
    return buffer;
  }
  /// Return a buffer to the pool.
  void release(T *buffer) { this->free_buffers_.push(buffer); }

 protected:
  std::queue<T *> free_buffers_;
};

}  // namespace esphome::camera
