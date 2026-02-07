#pragma once

#include "esphome/core/helpers.h"
#include <cstddef>
#include <cstdint>

namespace esphome::online_image {

/**
 * @brief Buffer for managing downloaded data.
 *
 * This class provides a buffer for downloading data with tracking of
 * unread bytes and dynamic resizing capabilities.
 */
class DownloadBuffer {
 public:
  DownloadBuffer(size_t size);
  ~DownloadBuffer() { this->allocator_.deallocate(this->buffer_, this->size_); }

  uint8_t *data(size_t offset = 0);
  uint8_t *append() { return this->data(this->unread_); }

  size_t unread() const { return this->unread_; }
  size_t size() const { return this->size_; }
  size_t free_capacity() const { return this->size_ - this->unread_; }

  size_t read(size_t len);
  size_t write(size_t len) {
    this->unread_ += len;
    return this->unread_;
  }

  void reset() { this->unread_ = 0; }
  size_t resize(size_t size);

 protected:
  RAMAllocator<uint8_t> allocator_{};
  uint8_t *buffer_;
  size_t size_;
  /** Total number of downloaded bytes not yet read. */
  size_t unread_;
};

}  // namespace esphome::online_image
