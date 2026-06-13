#include "download_buffer.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome::online_image {

static const char *const TAG = "online_image.download_buffer";

DownloadBuffer::DownloadBuffer(size_t size) : size_(size) {
  RAMAllocator<uint8_t> allocator;
  this->buffer_ = allocator.allocate(size);
  this->reset();
  if (!this->buffer_) {
    ESP_LOGE(TAG, "Initial allocation of download buffer failed!");
    this->size_ = 0;
  }
}

uint8_t *DownloadBuffer::data(size_t offset) {
  if (offset > this->size_) {
    ESP_LOGE(TAG, "Tried to access beyond download buffer bounds!!!");
    return this->buffer_;
  }
  return this->buffer_ + offset;
}

size_t DownloadBuffer::read(size_t len) {
  if (len >= this->unread_) {
    this->unread_ = 0;
    return 0;
  }
  this->unread_ -= len;
  memmove(this->data(), this->data(len), this->unread_);
  return this->unread_;
}

size_t DownloadBuffer::resize(size_t size) {
  if (this->size_ >= size) {
    // Avoid useless reallocations; if the buffer is big enough, don't reallocate.
    return this->size_;
  }
  RAMAllocator<uint8_t> allocator;
  allocator.deallocate(this->buffer_, this->size_);
  this->buffer_ = allocator.allocate(size);
  this->reset();
  if (this->buffer_) {
    this->size_ = size;
    return size;
  } else {
    ESP_LOGE(TAG, "allocation of %zu bytes failed. Biggest block in heap: %zu Bytes", size,
             allocator.get_max_free_block_size());
    this->size_ = 0;
    return 0;
  }
}

}  // namespace esphome::online_image
