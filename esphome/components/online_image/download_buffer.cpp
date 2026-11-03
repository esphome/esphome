#include "download_buffer.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cstring>

namespace esphome::online_image {

ESPHOME_LOG_TAG(TAG, "online_image.download_buffer");

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

void DownloadSource::compact() {
  if (this->keep_read_ || this->pos_ == this->start_)
    return;
  this->buffer_.read(this->pos_ - this->start_);
  this->start_ = this->pos_;
}

int32_t DownloadSource::read(uint8_t *buf, uint32_t len) {
  const size_t offset = this->pos_ - this->start_;
  const size_t n = std::min<size_t>(len, this->buffer_.unread() - offset);
  memcpy(buf, this->buffer_.data(offset), n);
  this->pos_ += n;
  return static_cast<int32_t>(n);
}

bool DownloadSource::seek(uint32_t position) {
  // Only what is still in the buffer can be read again; a decoder never skips ahead of the download
  if (position < this->start_ || position > this->start_ + this->buffer_.unread())
    return false;
  this->pos_ = position;
  return true;
}

bool DownloadBuffer::grow(size_t size) {
  if (this->size_ >= size)
    return true;
  RAMAllocator<uint8_t> allocator;
  uint8_t *buffer = allocator.allocate(size);
  if (buffer == nullptr) {
    ESP_LOGE(TAG, "allocation of %zu bytes failed. Biggest block in heap: %zu Bytes", size,
             allocator.get_max_free_block_size());
    return false;
  }
  memcpy(buffer, this->buffer_, this->unread_);
  allocator.deallocate(this->buffer_, this->size_);
  this->buffer_ = buffer;
  this->size_ = size;
  return true;
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
