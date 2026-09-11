#include "api_buffer.h"
#include <cstring>

namespace esphome::api {

bool APIBuffer::grow_(size_t n, size_t drop) {
  if (n > MAX_SIZE)
    return false;
  RAMAllocator<uint8_t> allocator;
  if (drop == 0) {
    // realloc extends in place when it can, avoiding the copy
    uint8_t *grown = allocator.reallocate(this->data_.get(), n);
    if (grown == nullptr)
      return false;
    (void) this->data_.release();  // realloc already freed or reused the old block
    this->data_.reset(grown);
    this->capacity_ = n;
    return true;
  }
  uint8_t *fresh = allocator.allocate(n);
  if (fresh == nullptr)
    return false;
  const size_t live = this->size_ - drop;
  if (live)
    std::memcpy(fresh, this->data_.get() + drop, live);
  this->data_.reset(fresh);
  this->capacity_ = n;
  this->size_ = live;
  return true;
}

bool APIBuffer::drop_front_and_reserve(size_t drop, size_t n) {
  if (n > this->capacity_)
    return this->grow_(n, drop);
  this->size_ -= drop;
  std::memmove(this->data_.get(), this->data_.get() + drop, this->size_);
  return true;
}

}  // namespace esphome::api
