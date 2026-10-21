#pragma once

#include "esphome/components/runtime_image/image_decoder.h"
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
  ~DownloadBuffer() {
    RAMAllocator<uint8_t> allocator;
    allocator.deallocate(this->buffer_, this->size_);
  }

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
  uint8_t *buffer_;
  size_t size_;
  /** Total number of downloaded bytes not yet read. */
  size_t unread_;
};

/**
 * @brief Lets a decoder read a download as it arrives, from the download buffer.
 *
 * The decoder reads at its own position in the buffer. Bytes it has read stay there until compact() drops
 * them, and compact() keeps everything while set_keep_read(true) is in effect, so the decoder can seek back
 * while it reads the headers.
 */
class DownloadSource : public runtime_image::DecodeSource {
 public:
  explicit DownloadSource(DownloadBuffer &buffer) : buffer_(buffer) {}

  /// Start again at the beginning of a new download, keeping what is read
  void reset() {
    this->start_ = 0;
    this->pos_ = 0;
    this->keep_read_ = true;
  }
  void set_keep_read(bool keep) { this->keep_read_ = keep; }
  /// Drop the bytes the decoder has read, to make room for more of the download
  void compact();

  int32_t read(uint8_t *buf, uint32_t len) override;
  bool seek(uint32_t position) override;

 protected:
  DownloadBuffer &buffer_;
  size_t start_{0};  // Position in the download of the first byte in the buffer
  size_t pos_{0};    // Position in the download the decoder reads next
  bool keep_read_{true};
};

}  // namespace esphome::online_image
