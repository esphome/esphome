#include "encoder_buffer_impl.h"

namespace esphome::camera_encoder {

bool EncoderBufferImpl::set_buffer_size(size_t size) {
  if (size > this->capacity_) {
    uint8_t *p = this->allocator_.reallocate(this->data_, size);
    if (p == nullptr)
      return false;

    this->data_ = p;
    this->capacity_ = size;
  }
  this->size_ = size;
  return true;
}

EncoderBufferImpl::~EncoderBufferImpl() {
  if (this->data_ != nullptr)
    this->allocator_.deallocate(this->data_, this->capacity_);
}

}  // namespace esphome::camera_encoder
