#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace esphome::split_buffer {
/**
 * A SplitBuffer allocates a large memory buffer potentially as multiple smaller buffers
 * to facilitate allocation of large buffers on devices with fragmented memory spaces.
 * Each sub-buffer is the same size, except for the last one which may be smaller.
 * Standard array indexing using `[]` is possible on the buffer, but, since the buffer may not be contiguous in memory,
 * there is no easy way to access the buffer as a single array, i.e. no `.data()` access like a vector.
 *
 * The sub-buffer size is always a power of two, so that `[]` resolves an index with a shift and a mask rather than a
 * division and a modulus. That matters: display drivers index a buffer of this kind once or twice per pixel, and a
 * 32-bit divide is tens of cycles on the cores these run on.
 *
 * Even so, `[]` is the wrong tool inside a per-pixel loop. Callers walking a run of bytes should ask span() for a raw
 * pointer once and then index that directly.
 */
class SplitBuffer {
 public:
  SplitBuffer() = default;
  ~SplitBuffer();

  // Initialize the buffer with the desired total length
  bool init(size_t total_length);

  // Free all allocated buffers
  void free();

  // Access operators
  uint8_t &operator[](size_t index) {
    // Safe: the object is not const, so neither is the byte the const overload found.
    return const_cast<uint8_t &>(static_cast<const SplitBuffer *>(this)->operator[](index));
  }
  const uint8_t &operator[](size_t index) const {
    if (index >= this->total_length_)
      return this->out_of_bounds_(index);
    return this->buffers_[index >> this->buffer_shift_][index & this->buffer_mask_];
  }
  void fill(uint8_t value) const;

  /**
   * Return a pointer to the byte at `index`, and in `length` the number of bytes following it that are contiguous in
   * memory - that is, up to the end of the sub-buffer holding it, or the end of the buffer, whichever comes first.
   *
   * For walking a run of bytes: resolve the pointer once and index it directly, rather than paying for the lookup in
   * `[]` on every byte. A caller that has consumed `length` bytes calls span() again for the next stretch.
   *
   * Returns nullptr, with `length` set to zero, if `index` is out of bounds.
   */
  uint8_t *span(size_t index, size_t &length) const {
    if (index >= this->total_length_) {
      length = 0;
      return nullptr;
    }
    const size_t offset = index & this->buffer_mask_;
    length = this->buffer_size_ - offset;
    if (const size_t to_end = this->total_length_ - index; length > to_end)
      length = to_end;
    return this->buffers_[index >> this->buffer_shift_] + offset;
  }

  // Get the total length
  size_t size() const { return this->total_length_; }

  // Get buffer information
  size_t get_buffer_count() const { return this->buffer_count_; }

  // Check if successfully initialized
  bool is_valid() const { return this->buffers_ != nullptr && this->buffer_count_ > 0; }

 private:
  const uint8_t &out_of_bounds_(size_t index) const;

  uint8_t **buffers_{nullptr};
  size_t buffer_count_{0};
  size_t buffer_size_{0};  // sub-buffer stride, always a power of two
  size_t buffer_mask_{0};  // buffer_size_ - 1
  size_t total_length_{0};
  uint8_t buffer_shift_{0};  // log2(buffer_size_)
};

}  // namespace esphome::split_buffer
