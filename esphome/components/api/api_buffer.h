#pragma once

#include <cstdint>
#include <cstring>
#include <memory>

#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

namespace esphome::api {

/// Byte buffer that skips zero-initialization on resize().
///
/// std::vector<uint8_t>::resize() zero-fills new bytes via memset. For the
/// shared protobuf write buffer, every byte is overwritten by the encoder,
/// making the zero-fill pure waste. For the receive buffer, bytes are
/// overwritten by socket reads.
///
/// Designed for bulk clear/resize/overwrite patterns. grow_() allocates
/// exactly the requested size (no growth factor) since callers resize to
/// known sizes rather than appending incrementally.
///
/// Safe because: callers always write exactly the number of bytes they
/// resize for. In the protobuf write path, debug_check_bounds_ validates
/// writes in debug builds.
class APIBuffer {
 public:
  void clear() { this->size_ = 0; }
  /// Returns false if allocation fails; the buffer is left unchanged.
  [[nodiscard]] inline bool reserve(size_t n) ESPHOME_ALWAYS_INLINE { return n <= this->capacity_ || this->grow_(n); }
  /// Returns false if allocation fails; the buffer is left unchanged. No zero-fill.
  [[nodiscard]] inline bool resize(size_t n) ESPHOME_ALWAYS_INLINE { return this->reserve_and_resize(n, n); }
  /// Reserve capacity for max(reserve_size, new_size) bytes, then set size to new_size.
  /// Single grow_ check regardless of argument order.
  /// Returns false if allocation fails; the buffer is left unchanged.
  [[nodiscard]] inline bool reserve_and_resize(size_t reserve_size, size_t new_size) ESPHOME_ALWAYS_INLINE {
    if (!this->reserve(std::max(reserve_size, new_size)))
      return false;
    this->size_ = new_size;
    return true;
  }
  /// Grow by n bytes; returns the new bytes, or nullptr on allocation failure.
  [[nodiscard]] uint8_t *append(size_t n, size_t reserve_size = 0) {
    const size_t old_size = this->size_;
    if (!this->reserve_and_resize(reserve_size, old_size + n))
      return nullptr;
    return this->data_.get() + old_size;
  }
  /// Drop the first `drop` bytes and reserve `n`, copying the rest only once.
  /// Returns false on allocation failure.
  [[nodiscard]] bool drop_front_and_reserve(size_t drop, size_t n);
  uint8_t *data() { return this->data_.get(); }
  const uint8_t *data() const { return this->data_.get(); }
  size_t size() const { return this->size_; }
  size_t capacity() const { return this->capacity_; }
  bool empty() const { return this->size_ == 0; }
  uint8_t &operator[](size_t i) { return this->data_[i]; }
  const uint8_t &operator[](size_t i) const { return this->data_[i]; }
  /// Release all memory (equivalent to std::vector swap trick).
  void release() {
    this->data_.reset();
    this->size_ = 0;
    this->capacity_ = 0;
  }

 protected:
  bool grow_(size_t n, size_t drop = 0);
  std::unique_ptr<uint8_t[]> data_;
  size_t size_{0};
  size_t capacity_{0};
};

}  // namespace esphome::api
