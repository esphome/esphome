#include "buffer_impl.h"

namespace esphome::camera {

BufferImpl::BufferImpl(size_t size) {
  this->data_ = this->allocator_.allocate(size);
  this->size_ = size;
}

BufferImpl::BufferImpl(CameraImageSpec *spec) {
  this->data_ = this->allocator_.allocate(spec->bytes_per_image());
  this->size_ = spec->bytes_per_image();
}

BufferImpl::~BufferImpl() {
  if (this->data_ != nullptr)
    this->allocator_.deallocate(this->data_, this->size_);
}

}  // namespace esphome::camera
