#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

namespace esphome {
namespace packet_interface {

/**
 * A buffer class that efficiently represents a composite buffer with three sections:
 * - pre_buffer: A pointer to uint8_t data with a length
 * - mid_buffer: A reference to another PacketBuffer (optional)
 * - post_buffer: A pointer to uint8_t data with a length
 *
 * This allows efficient zero-copy composition of buffers without allocating new memory.
 */
class PacketBuffer {
 public:
  PacketBuffer() = default;

  // Disable copy semantics - PacketBuffer contains pointers and copying would create dangling references
  PacketBuffer(const PacketBuffer &) = delete;
  PacketBuffer &operator=(const PacketBuffer &) = delete;

  // Enable move semantics
  PacketBuffer(PacketBuffer &&) = default;
  PacketBuffer &operator=(PacketBuffer &&) = default;

  /**
   * Construct from a single buffer.
   */
  PacketBuffer(const uint8_t *data, size_t length) : pre_buffer_(data), pre_length_(length) {}

  /**
   * Construct from a vector.
   */
  explicit PacketBuffer(const std::vector<uint8_t> &vec) : pre_buffer_(vec.data()), pre_length_(vec.size()) {}

  /**
   * Construct with pre and post buffers.
   */
  PacketBuffer(const uint8_t *pre_data, size_t pre_length, const uint8_t *post_data, size_t post_length)
      : pre_buffer_(pre_data), pre_length_(pre_length), post_buffer_(post_data), post_length_(post_length) {}

  /**
   * Construct with pre, mid, and post buffers.
   */
  PacketBuffer(const uint8_t *pre_data, size_t pre_length, const PacketBuffer *mid_buffer, const uint8_t *post_data,
               size_t post_length)
      : pre_buffer_(pre_data),
        pre_length_(pre_length),
        mid_buffer_(mid_buffer),
        post_buffer_(post_data),
        post_length_(post_length) {}

  /**
   * Get the total size of the buffer.
   */
  size_t size() const {
    size_t total = this->pre_length_ + this->post_length_;
    if (this->mid_buffer_ != nullptr) {
      total += this->mid_buffer_->size();
    }
    return total;
  }

  /**
   * Check if the buffer is empty.
   */
  bool empty() const { return this->size() == 0; }

  /**
   * Access a byte at a specific position.
   * @param index The position to access
   * @return The byte at the specified position
   */
  uint8_t operator[](size_t index) const {
    if (index < this->pre_length_) {
      return this->pre_buffer_[index];
    }
    index -= this->pre_length_;

    if (this->mid_buffer_ != nullptr) {
      size_t mid_size = this->mid_buffer_->size();
      if (index < mid_size) {
        return (*this->mid_buffer_)[index];
      }
      index -= mid_size;
    }

    return this->post_buffer_[index];
  }

  /**
   * Copy data from the buffer to a destination pointer.
   * @param dest Destination pointer
   * @param count Number of bytes to copy
   * @param offset Offset in the source buffer to start copying from
   * @return Number of bytes actually copied
   */
  size_t copy_to(uint8_t *dest, size_t count, size_t offset = 0) const {
    size_t total_size = this->size();
    if (offset >= total_size) {
      return 0;
    }

    size_t available = total_size - offset;
    size_t to_copy = count < available ? count : available;
    size_t copied = 0;

    // Copy from pre_buffer
    if (offset < this->pre_length_) {
      size_t pre_offset = offset;
      size_t pre_available = this->pre_length_ - pre_offset;
      size_t pre_copy = to_copy < pre_available ? to_copy : pre_available;
      std::memcpy(dest, this->pre_buffer_ + pre_offset, pre_copy);
      copied += pre_copy;
      dest += pre_copy;
      offset += pre_copy;
    }

    // Copy from mid_buffer
    if (copied < to_copy && this->mid_buffer_ != nullptr) {
      size_t mid_offset = offset - this->pre_length_;
      size_t mid_available = this->mid_buffer_->size() - mid_offset;
      size_t mid_copy = (to_copy - copied) < mid_available ? (to_copy - copied) : mid_available;
      if (mid_copy > 0) {
        this->mid_buffer_->copy_to(dest, mid_copy, mid_offset);
        copied += mid_copy;
        dest += mid_copy;
        offset += mid_copy;
      }
    }

    // Copy from post_buffer
    if (copied<to_copy &&this->post_length_> 0) {
      size_t post_offset = offset - this->pre_length_;
      if (this->mid_buffer_ != nullptr) {
        post_offset -= this->mid_buffer_->size();
      }
      size_t post_available = this->post_length_ - post_offset;
      size_t post_copy = (to_copy - copied) < post_available ? (to_copy - copied) : post_available;
      std::memcpy(dest, this->post_buffer_ + post_offset, post_copy);
      copied += post_copy;
    }

    return copied;
  }

  /**
   * Convert the buffer to a std::vector.
   */
  std::vector<uint8_t> to_vector() const {
    std::vector<uint8_t> result;
    size_t total_size = this->size();
    result.resize(total_size);
    this->copy_to(result.data(), total_size);
    return result;
  }

  /**
   * Create a PacketBuffer from a vector (convenience method).
   */
  static PacketBuffer from_vector(const std::vector<uint8_t> &vec) { return PacketBuffer(vec); }

  /**
   * Assignment operator from std::vector.
   * Creates a PacketBuffer that references the vector's data.
   */
  PacketBuffer &operator=(const std::vector<uint8_t> &vec) {
    this->pre_buffer_ = vec.data();
    this->pre_length_ = vec.size();
    this->mid_buffer_ = nullptr;
    this->post_buffer_ = nullptr;
    this->post_length_ = 0;
    return *this;
  }

  /**
   * Conversion operator to std::vector.
   * Creates a copy of the buffer data.
   */
  operator std::vector<uint8_t>() const { return this->to_vector(); }

  /**
   * Iterator class for PacketBuffer.
   */
  class Iterator {
   public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = uint8_t;
    using difference_type = std::ptrdiff_t;
    using pointer = const uint8_t *;
    using reference = uint8_t;

    Iterator(const PacketBuffer *buffer, size_t index) : buffer_(buffer), index_(index) {}

    reference operator*() const { return (*this->buffer_)[this->index_]; }

    const Iterator &operator++() {
      ++this->index_;
      return *this;
    }

    const Iterator operator++(int) {
      const Iterator tmp = *this;
      ++(*this);
      return tmp;
    }

    bool operator==(const Iterator &other) const {
      return this->buffer_ == other.buffer_ && this->index_ == other.index_;
    }

    bool operator!=(const Iterator &other) const { return !(*this == other); }

   private:
    const PacketBuffer *buffer_;
    size_t index_;
  };

  /**
   * Begin iterator.
   */
  Iterator begin() const { return Iterator(this, 0); }

  /**
   * End iterator.
   */
  Iterator end() const { return Iterator(this, this->size()); }

  /**
   * Set the pre-buffer.
   */
  void set_pre_buffer(const uint8_t *data, size_t length) {
    this->pre_buffer_ = data;
    this->pre_length_ = length;
  }

  /**
   * Set the mid-buffer.
   */
  void set_mid_buffer(const PacketBuffer *buffer) { this->mid_buffer_ = buffer; }

  /**
   * Set the post-buffer.
   */
  void set_post_buffer(const uint8_t *data, size_t length) {
    this->post_buffer_ = data;
    this->post_length_ = length;
  }

 private:
  const uint8_t *pre_buffer_{nullptr};
  size_t pre_length_{0};
  const PacketBuffer *mid_buffer_{nullptr};
  const uint8_t *post_buffer_{nullptr};
  size_t post_length_{0};
};

}  // namespace packet_interface
}  // namespace esphome
