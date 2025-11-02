#include "buffer_impl.h"

#include "esp_heap_caps.h"

namespace esphome::camera {

BufferImpl::BufferImpl() { this->allocator_.set_caps(MALLOC_CAP_CACHE_ALIGNED | MALLOC_CAP_8BIT); }

BufferImpl::BufferImpl(size_t size) {
  this->allocator_.set_caps(MALLOC_CAP_CACHE_ALIGNED | MALLOC_CAP_8BIT);
  this->data_ = this->allocator_.allocate(size);
  this->size_ = size;
  this->capacity_ = this->size_;
}

BufferImpl::BufferImpl(CameraImageSpec *spec) {
  this->allocator_.set_caps(MALLOC_CAP_CACHE_ALIGNED | MALLOC_CAP_8BIT);
  this->data_ = this->allocator_.allocate(spec->bytes_per_image());
  this->size_ = spec->bytes_per_image();
  this->capacity_ = this->size_;
}

bool BufferImpl::set_buffer_size(size_t size) {
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

BufferImpl::~BufferImpl() { this->allocator_.deallocate(this->data_, this->size_); }

}  // namespace esphome::camera
