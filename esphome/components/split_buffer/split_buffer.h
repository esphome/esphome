#pragma once

#include <cstdint>
#include <cstdlib>

namespace esphome::split_buffer {

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
