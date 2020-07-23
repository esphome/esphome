#include "ring_buffer.h"

#include <algorithm>

namespace esphome {
namespace tcp {

RingBuffer::~RingBuffer() {
   delete[] this->buffer_;
}

void RingBuffer::reserve(size_t size) {
  if (size <= this->capacity_)
    return;
  // first linearize to front so that we can copy the array safely
  this->linearize();

  // find new capacity (power of 2)
  if (this->capacity_ == 0)
    this->capacity_ = 32;
  while (size > this->capacity_)
    this->capacity_ *= 2;

  auto *new_buffer = new uint8_t[this->capacity_];
  auto *old_buffer = this->buffer_;
  std::copy(old_buffer, old_buffer + this->size_, new_buffer);
  delete[] old_buffer;
  this->buffer_ = new_buffer;
}

void RingBuffer::linearize() {
  std::rotate(this->buffer_, this->buffer_ + this->start_, this->buffer_ + this->capacity_);
  this->start_ = 0;
}

void RingBuffer::push_back(const uint8_t *items, size_t size) {
  this->reserve(this->size_ + size);

  size_t index = (this->start_ + this->size_) % this->capacity_;
  for (size_t i = 0; i < size; i++) {
    this->buffer_[index++] = items[i];
    if (index == this->capacity_)
      index = 0;
  }
  this->size_ += size;
}

std::pair<const uint8_t *, size_t> RingBuffer::pop_front_linear(size_t max_size) {
  size_t left_at_end = this->capacity_ - this->start_;
  size_t to_pop = std::min(left_at_end, max_size);
  uint8_t *data = this->buffer_ + this->start_;
  this->start_ = (this->start_ + to_pop) % this->capacity_;
  this->size_ -= to_pop;
  return {data, to_pop};
}

std::pair<const uint8_t *, size_t> RingBuffer::peek_front_linear(size_t max_size) {
  size_t left_at_end = this->capacity_ - this->start_;
  size_t to_peek = std::min(left_at_end, max_size);
  uint8_t *data = this->buffer_ + this->start_;
  return {data, to_peek};
}

}  // namepsace tcp
}  // namespace esphome
