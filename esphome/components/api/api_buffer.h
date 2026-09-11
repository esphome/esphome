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
  static constexpr size_t MAX_SIZE = UINT16_MAX;  // API frames carry 16 bit lengths
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
    this->size_ = static_cast<uint16_t>(new_size);
    return true;
  }
  /// Grow by n bytes; returns the new bytes, or nullptr on allocation failure.
  [[nodiscard]] uint8_t *append(size_t n);
  /// Drop the first `drop` bytes (at most size()), sliding the rest down.
  void drop_front(size_t drop) {
#ifdef ESPHOME_DEBUG_API
    this->debug_check_drop_(drop);
#endif
    this->size_ -= drop;
    std::memmove(this->data_.get(), this->data_.get() + drop, this->size_);
  }
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
  bool grow_(size_t n);
#ifdef ESPHOME_DEBUG_API
  void debug_check_drop_(size_t drop) const;
#endif
  // RAMAllocator: PSRAM when available, and it reports failure where
  // new (std::nothrow) still aborts on ESP-IDF without exceptions
  struct FreeDeleter {
    void operator()(uint8_t *p) const { RAMAllocator<uint8_t>().deallocate(p, 0); }
  };
  std::unique_ptr<uint8_t[], FreeDeleter> data_;
  uint16_t size_{0};
  uint16_t capacity_{0};
};

}  // namespace esphome::api
