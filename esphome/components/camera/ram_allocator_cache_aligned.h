#pragma once

#include "esphome/core/helpers.h"

namespace esphome::camera {

/// RAM allocator that provides cache-aligned memory for DMA and hardware-accelerated operations.
template<class T> class RAMAllocatorCacheAligned : public RAMAllocator<T> {
 public:
  void set_caps(uint32_t caps) { this->caps_ = caps; }

  T *allocate(size_t n) { return this->allocate(n, sizeof(T)); }

  T *allocate(size_t n, size_t manual_size) {
    size_t size = n * manual_size;
    T *ptr = nullptr;

    if (this->protected_flags_ & RAMAllocator<T>::Flags::ALLOC_EXTERNAL) {
      ptr = static_cast<T *>(heap_caps_malloc(size, MALLOC_CAP_SPIRAM | this->caps_));
    }
    if (ptr == nullptr && this->protected_flags_ & RAMAllocator<T>::Flags::ALLOC_INTERNAL) {
      ptr = static_cast<T *>(heap_caps_malloc(size, MALLOC_CAP_INTERNAL | this->caps_));
    }
    return ptr;
  }

  T *reallocate(T *p, size_t n) { return this->reallocate(p, n, sizeof(T)); }

  T *reallocate(T *p, size_t n, size_t manual_size) {
    size_t size = n * manual_size;
    T *ptr = nullptr;
    if (this->protected_flags_ & RAMAllocator<T>::Flags::ALLOC_EXTERNAL) {
      ptr = static_cast<T *>(heap_caps_realloc(p, size, MALLOC_CAP_SPIRAM | this->caps_));
    }
    if (ptr == nullptr && this->protected_flags_ & RAMAllocator<T>::Flags::ALLOC_INTERNAL) {
      ptr = static_cast<T *>(heap_caps_realloc(p, size, MALLOC_CAP_INTERNAL | this->caps_));
    }
    return ptr;
  }

 protected:
  uint8_t protected_flags_{RAMAllocator<T>::ALLOC_INTERNAL | RAMAllocator<T>::ALLOC_EXTERNAL};
  uint32_t caps_{MALLOC_CAP_8BIT};
};

}  // namespace esphome::camera
