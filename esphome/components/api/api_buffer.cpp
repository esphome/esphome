#include "api_buffer.h"
#include <cstdlib>

namespace esphome::api {

bool APIBuffer::grow_(size_t n, size_t drop) {
  if (n > UINT16_MAX)
    return false;
  if (drop == 0) {
    // realloc extends in place when it can, avoiding the copy
    auto *grown = static_cast<uint8_t *>(std::realloc(this->data_.get(), n));  // NOLINT(cppcoreguidelines-no-malloc)
    if (grown == nullptr)
      return false;
    (void) this->data_.release();  // realloc already freed or reused the old block
    this->data_.reset(grown);
    this->capacity_ = static_cast<uint16_t>(n);
    return true;
  }
  auto *fresh = static_cast<uint8_t *>(std::malloc(n));  // NOLINT(cppcoreguidelines-no-malloc)
  if (fresh == nullptr)
    return false;
  const uint16_t live = this->size_ - static_cast<uint16_t>(drop);
  if (live)
    std::memcpy(fresh, this->data_.get() + drop, live);
  this->data_.reset(fresh);
  this->capacity_ = static_cast<uint16_t>(n);
  this->size_ = live;
  return true;
}

bool APIBuffer::drop_front_and_reserve(size_t drop, size_t n) {
  if (n > this->capacity_)
    return this->grow_(n, drop);
  this->size_ -= static_cast<uint16_t>(drop);
  std::memmove(this->data_.get(), this->data_.get() + drop, this->size_);
  return true;
}

}  // namespace esphome::api
