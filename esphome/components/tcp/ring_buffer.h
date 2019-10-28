#pragma once

#include <algorithm>

namespace esphome {
namespace tcp {

template<typename T> class VectorRingBuffer {
 public:
  virtual ~VectorRingBuffer() { delete[] this->buffer_; }
  void reserve(size_t size) {
    if (size <= this->capacity_)
      return;
    // first linearlize to front so that we can copy the array safely
    this->linearlize();

    // find new capacity (power of 2)
    if (this->capacity_ == 0)
      this->capacity_ = 32;
    while (size > this->capacity_)
      this->capacity_ *= 2;

    auto *new_buffer = new T[this->capacity_];
    auto *old_buffer = this->buffer_;
    std::copy(old_buffer, old_buffer + this->size_, new_buffer);
    delete[] old_buffer;
    this->buffer_ = new_buffer;
  }
  size_t capacity() { return this->capacity_; }
  size_t size() { return this->size_; }
  bool empty() { return this->size_ == 0; }

  void linearlize() {
    std::rotate(this->buffer_, this->buffer_ + this->start_, this->buffer_ + this->capacity_);
    this->start_ = 0;
  }

  void push_back(T item) {
    this->reserve(this->size_ + 1);
    size_t index = (this->start_ + this->size_) % this->capacity_;
    this->buffer_[index] = item;
    this->size_++;
  }
  void push_back(const T *items, size_t size) {
    this->reserve(this->size_ + size);
    for (size_t i = 0; i < size; i++)
      this->push_back(items[i]);
  }
  T pop_front() {
    T item = this->buffer_[this->start_];
    this->start_ = (this->start_ + 1) % this->capacity_;
    this->size_--;
    return item;
  }
  std::pair<T *, size_t> pop_front_linear(size_t max_size) {
    size_t left_at_end = this->capacity_ - this->start_;
    size_t to_pop = std::min(left_at_end, max_size);
    T *data = this->buffer_ + this->start_;
    this->start_ = (this->start_ + to_pop) % this->capacity_;
    this->size_ -= to_pop;
    return std::make_pair(data, to_pop);
  }
  std::pair<T *, size_t> peek_front_linear(size_t max_size) {
    size_t left_at_end = this->capacity_ - this->start_;
    size_t to_peek = std::min(left_at_end, max_size);
    T *data = this->buffer_ + this->start_;
    return std::make_pair(data, to_peek);
  }

 protected:
  size_t start_{0};
  size_t size_{0};
  T *buffer_{nullptr};
  size_t capacity_{0};
};

}  // namespace tcp
}  // namespace esphome
