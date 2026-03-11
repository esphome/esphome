#pragma once

#include <cstdint>
#include <cstring>
#include <memory>

#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

namespace esphome::api {

/// Helper to use make_unique_for_overwrite where available (skips zero-fill),
/// falling back to make_unique on older GCC (ESP8266, LibreTiny).
inline std::unique_ptr<uint8_t[]> make_buffer(size_t n) {
#if defined(USE_ESP8266) || defined(USE_LIBRETINY)
  return std::make_unique<uint8_t[]>(n);
#else
  return std::make_unique_for_overwrite<uint8_t[]>(n);
#endif
}

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
  inline void reserve(size_t n) ESPHOME_ALWAYS_INLINE {
    if (n > this->capacity_)
      this->grow_(n);
  }
  inline void resize(size_t n) ESPHOME_ALWAYS_INLINE {
    this->reserve(n);
    this->size_ = n;  // no zero-fill
  }
  uint8_t *data() { return this->data_.get(); }
  const uint8_t *data() const { return this->data_.get(); }
  size_t size() const { return this->size_; }
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
  void grow_(size_t n);
  std::unique_ptr<uint8_t[]> data_;
  size_t size_{0};
  size_t capacity_{0};
};

}  // namespace esphome::api
