#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>

namespace esphome {
namespace tcp {

/** A ring buffer of bytes.
 */
class RingBuffer {
 public:
  ~RingBuffer();

  /// Ensure the buffer contains
  void reserve(size_t size);
  size_t capacity() { return this->capacity_; }
  size_t size() { return this->size_; }
  bool empty() { return this->size_ == 0; }

  void linearize();

  void push_back(const uint8_t *items, size_t size);
  std::pair<const uint8_t *, size_t> pop_front_linear(size_t max_size);
  std::pair<const uint8_t *, size_t> peek_front_linear(size_t max_size);

 protected:
  size_t start_{0};
  size_t size_{0};
  uint8_t *buffer_{nullptr};
  size_t capacity_{0};
};

}  // namespace tcp
}  // namespace esphome
