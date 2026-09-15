#include "split_buffer.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::split_buffer {
static constexpr const char *const TAG = "split_buffer";

SplitBuffer::~SplitBuffer() { this->free(); }

bool SplitBuffer::init(size_t total_length) {
  this->free();  // Clean up any existing allocation

  if (total_length == 0) {
    return false;
  }

  this->total_length_ = total_length;

  RAMAllocator<uint8_t *> ptr_allocator;
  RAMAllocator<uint8_t> allocator;

  // Strides are powers of two, so that indexing costs a shift and a mask instead of a divide and a modulus. Start at
  // the smallest one that covers the whole length - a single allocation, and the only case where the stride is larger
  // than the buffer - then halve it on each failure, as the fragmented heap this class exists for demands.
  uint8_t shift = 0;
  while (((size_t) 1 << shift) < total_length && shift < sizeof(size_t) * 8 - 1) {
    shift++;
  }

  while (true) {
    const size_t stride = (size_t) 1 << shift;
    // Calculate how many buffers we need of this size
    size_t needed_buffers = (total_length + stride - 1) / stride;

    // Try to allocate array of buffer pointers
    uint8_t **temp_buffers = ptr_allocator.allocate(needed_buffers);
    if (temp_buffers == nullptr) {
      // If we can't even allocate the pointer array, don't need to continue
      ESP_LOGE(TAG, "Failed to allocate pointers");
      return false;
    }

    // Initialize all pointers to null
    for (size_t i = 0; i < needed_buffers; i++) {
      temp_buffers[i] = nullptr;
    }

    // Try to allocate all the buffers
    bool allocation_success = true;
    for (size_t i = 0; i < needed_buffers; i++) {
      size_t this_buffer_size = stride;
      // Last buffer might be smaller if total_length is not divisible by the stride
      if (i == needed_buffers - 1 && total_length % stride != 0) {
        this_buffer_size = total_length % stride;
      }

      temp_buffers[i] = allocator.allocate(this_buffer_size);
      if (temp_buffers[i] == nullptr) {
        allocation_success = false;
        break;
      }

      // Initialize buffer to zero
      memset(temp_buffers[i], 0, this_buffer_size);
    }

    if (allocation_success) {
      // Success! Store the result
      this->buffers_ = temp_buffers;
      this->buffer_count_ = needed_buffers;
      this->buffer_size_ = stride;
      this->buffer_mask_ = stride - 1;
      this->buffer_shift_ = shift;
      ESP_LOGD(TAG, "Allocated %zu * %zu bytes - %zu bytes", this->buffer_count_, this->buffer_size_,
               this->total_length_);
      return true;
    }

    // Allocation failed, clean up and try smaller buffers
    for (size_t i = 0; i < needed_buffers; i++) {
      if (temp_buffers[i] != nullptr) {
        allocator.deallocate(temp_buffers[i], 0);
      }
    }
    ptr_allocator.deallocate(temp_buffers, 0);

    if (shift == 0) {
      break;
    }
    shift--;
  }

  ESP_LOGE(TAG, "Failed to allocate %zu bytes", total_length);
  return false;
}

void SplitBuffer::free() {
  if (this->buffers_ != nullptr) {
    RAMAllocator<uint8_t> allocator;
    for (size_t i = 0; i < this->buffer_count_; i++) {
      if (this->buffers_[i] != nullptr) {
        allocator.deallocate(this->buffers_[i], 0);
      }
    }
    RAMAllocator<uint8_t *> ptr_allocator;
    ptr_allocator.deallocate(this->buffers_, 0);
    this->buffers_ = nullptr;
  }
  this->buffer_count_ = 0;
  this->buffer_size_ = 0;
  this->buffer_mask_ = 0;
  this->buffer_shift_ = 0;
  this->total_length_ = 0;
}

// Out of line and off the hot path: `[]` is inlined in the header, and this is the branch it must not carry.
const uint8_t &SplitBuffer::out_of_bounds_(size_t index) const {
  ESP_LOGE(TAG, "Out of bounds - %zu >= %zu", index, this->total_length_);
  // Return reference to a static dummy byte since we can't throw exceptions.
  // the byte is non-const since it will also be used by the non-const [] overload.
  static uint8_t dummy = 0;
  return dummy;
}

/**
 * Fill the entire buffer with a single byte value
 * @param value Fill value
 */
void SplitBuffer::fill(uint8_t value) const {
  if (this->buffer_count_ == 0)
    return;
  // clear all the full sized buffers
  size_t i = 0;
  for (; i != this->buffer_count_ - 1; i++) {
    memset(this->buffers_[i], value, this->buffer_size_);
  }
  // clear the last, potentially short, buffer.
  // `i` is guaranteed to equal the last index since the loop terminates at that value.
  // where all buffers are the same size, the masked value must return the size, not 0.
  auto size_last = ((this->total_length_ - 1) & this->buffer_mask_) + 1;
  memset(this->buffers_[i], value, size_last);
}

}  // namespace esphome::split_buffer
