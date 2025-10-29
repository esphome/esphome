#pragma once

#include <cstdint>
#include <cstdlib>

namespace esphome::split_buffer {
/**
 * A SplitBuffer allocates a large memory buffer potentially as multiple smaller buffers
 * to facilitate allocation of large buffers on devices with fragmented memory spaces.
 * Each sub-buffer is the same size.
 * Standard array indexing using `[]` is possible on the buffer, but since the buffer may not be contiguous in memory so
 * there is no easy way to access the buffer as a single array, i.e. no `.data()` access like a vector.
 */
class SplitBuffer {
 public:
  SplitBuffer() = default;
  ~SplitBuffer();

  // Initialize the buffer with the desired total length
  bool init(size_t total_length);

  // Free all allocated buffers
  void free();

  // Access operators
  uint8_t &operator[](size_t index);
  const uint8_t &operator[](size_t index) const;
  void fill(uint8_t value);

  // Get the total length
  size_t size() const { return this->total_length_; }

  // Get buffer information
  size_t get_buffer_count() const { return this->buffer_count_; }
  size_t get_buffer_size() const { return this->buffer_size_; }

  // Check if successfully initialized
  bool is_valid() const { return this->buffers_ != nullptr && this->buffer_count_ > 0; }

 private:
  uint8_t **buffers_{nullptr};
  size_t buffer_count_{0};
  size_t buffer_size_{0};
  size_t total_length_{0};
};

}  // namespace esphome::split_buffer
