#include "buffer_impl.h"

namespace esphome::camera {

BufferImpl::BufferImpl(size_t size) {
  RAMAllocator<uint8_t> allocator;
  this->data_ = allocator.allocate(size);
  this->size_ = size;
}

BufferImpl::BufferImpl(CameraImageSpec *spec) {
  RAMAllocator<uint8_t> allocator;
  this->data_ = allocator.allocate(spec->bytes_per_image());
  this->size_ = spec->bytes_per_image();
}

BufferImpl::~BufferImpl() {
  if (this->data_ != nullptr) {
    RAMAllocator<uint8_t> allocator;
    allocator.deallocate(this->data_, this->size_);
  }
}

}  // namespace esphome::camera
