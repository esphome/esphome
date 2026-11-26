#include "lzss.h"
#include <algorithm>
#include <bit>
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

namespace esphome::lzss {

// Gamma codes up to this many bits after the first, so lengths up to 65536
static constexpr uint8_t MAX_GAMMA_ZEROS = 15;
static_assert(Lzss::MAX_MATCH == size_t{1} << (MAX_GAMMA_ZEROS + 1));
static constexpr size_t LITERAL_BITS = 9;
static constexpr uint16_t NO_POSITION = 0xFFFF;

static size_t gamma_bits(size_t value) { return 2 * std::bit_width(value) - 1; }

namespace {

// Writes bits from bit 7 of each byte, and stops writing once the output is full
class BitWriter {
 public:
  BitWriter(uint8_t *out, size_t capacity) : out_(out), capacity_(capacity) {}

  void put(uint32_t value, size_t count) {
    while (count-- != 0) {
      if (this->used_ == 0) {
        if (this->length_ == this->capacity_) {
          this->full_ = true;
          return;
        }
        this->out_[this->length_] = 0;
      }
      if ((value >> count) & 1)
        this->out_[this->length_] |= 0x80 >> this->used_;
      if (++this->used_ == 8) {
        this->used_ = 0;
        this->length_++;
      }
    }
  }
  void put_gamma(uint32_t value) {
    size_t width = std::bit_width(value);
    this->put(0, width - 1);
    this->put(value, width);
  }
  bool is_full() const { return this->full_; }
  /// Pads the last byte with 1 bits; returns the length, or 0 if the output was too small.
  size_t finish() {
    if (this->full_)
      return 0;
    if (this->used_ != 0)
      this->out_[this->length_++] |= 0xFF >> this->used_;
    return this->length_;
  }

 protected:
  uint8_t *out_;
  size_t capacity_;
  size_t length_{0};
  uint8_t used_{0};
  bool full_{false};
};

class BitReader {
 public:
  BitReader(const uint8_t *data, size_t size) : data_(data), end_(size) {}

  size_t left() const { return (this->end_ - this->pos_) * 8 - this->used_; }
  /// Only call with no more than left() bits.
  uint32_t get(size_t count) {
    uint32_t value = 0;
    while (count-- != 0) {
      if (this->used_ == 0)
        this->byte_ = this->data_[this->pos_];
      value = value << 1 | ((this->byte_ >> (7 - this->used_)) & 1);
      if (++this->used_ == 8) {
        this->used_ = 0;
        this->pos_++;
      }
    }
    return value;
  }

 protected:
  const uint8_t *data_;
  size_t pos_{0};
  size_t end_;
  uint8_t byte_{0};
  uint8_t used_{0};
};

}  // namespace

uint8_t Lzss::byte_at_(const uint8_t *data, size_t pos) const {
  return pos < this->dictionary_size_ ? progmem_read_byte(this->dictionary_ + pos) : data[pos - this->dictionary_size_];
}

size_t Lzss::compress(const uint8_t *in, size_t size, uint8_t *out, size_t capacity, uint16_t *chains) const {
  BitWriter writer(out, capacity);
  size_t total = this->dictionary_size_ + size;
  // For each hash, the latest position with it; for each of the last WINDOW positions, the one before it with the
  // same hash
  uint16_t *head = chains;
  uint16_t *previous = chains + (size_t{1} << HASH_BITS);
  auto hash = [&](size_t pos) {
    return ((this->byte_at_(in, pos) << 8 | this->byte_at_(in, pos + 1)) * 0x9E3779B1u) >> (32 - HASH_BITS);
  };
  auto insert = [&](size_t pos) {
    if (pos + 1 >= total)
      return;
    size_t h = hash(pos);
    previous[pos & (WINDOW - 1)] = head[h];
    head[h] = pos;
  };
  std::fill_n(head, size_t{1} << HASH_BITS, NO_POSITION);
  // A match reaches back at most WINDOW bytes, so earlier dictionary bytes are never used
  for (size_t pos = this->dictionary_size_ > WINDOW ? this->dictionary_size_ - WINDOW : 0; pos < this->dictionary_size_;
       pos++)
    insert(pos);
  size_t i = 0;
  while (i < size && !writer.is_full()) {
    size_t pos = this->dictionary_size_ + i;
    size_t longest = clamp_at_most(size - i, MAX_MATCH);
    size_t best_length = 0;
    size_t best_offset = 0;
    if (longest >= 2) {
      // The chain runs from the nearest, so of two matches of the same length the nearer is kept. Positions are
      // kept modulo 65536, so each entry gives only a distance, and a stale one still names bytes that are compared
      const size_t reach = clamp_at_most(pos, WINDOW);
      size_t last = 0;
      size_t tries = MAX_CHAIN;
      for (uint16_t entry = head[hash(pos)]; tries-- != 0;) {
        const size_t distance = static_cast<uint16_t>(pos - entry);
        // A chain only reaches further back, so a nearer entry is a slot reused by a later position
        if (distance <= last || distance > reach)
          break;
        last = distance;
        const size_t ref = pos - distance;
        size_t length = 0;
        // A match may run on past pos, copying bytes the expander has just written
        while (length < longest && this->byte_at_(in, ref + length) == in[i + length])
          length++;
        if (length > best_length) {
          best_length = length;
          best_offset = distance;
          if (length == longest)
            break;
        }
        entry = previous[ref & (WINDOW - 1)];
      }
    }
    size_t step = 1;
    const size_t offset_bits = std::bit_width(clamp_at_most(pos, WINDOW) - 1);
    if (best_length >= 2 && 1 + offset_bits + gamma_bits(best_length - 1) < LITERAL_BITS * best_length) {
      writer.put(0, 1);
      writer.put(best_offset - 1, offset_bits);
      writer.put_gamma(best_length - 1);
      step = best_length;
    } else {
      writer.put(1, 1);
      writer.put(in[i], 8);
    }
    for (size_t end = pos + step; pos != end; pos++)
      insert(pos);
    i += step;
  }
  return writer.finish();
}

size_t Lzss::expand(const uint8_t *in, size_t size, uint8_t *out, size_t capacity) const {
  BitReader reader(in, size);
  size_t length = 0;
  while (reader.left() != 0) {
    if (reader.get(1) != 0) {
      size_t left = reader.left();
      if (left < 8) {
        // The padding: the rest must be 1 bits
        return reader.get(left) == (1u << left) - 1 ? length : SIZE_MAX;
      }
      if (length == capacity)
        return SIZE_MAX;
      out[length++] = reader.get(8);
      continue;
    }
    size_t behind = this->dictionary_size_ + length;
    if (behind == 0)
      return SIZE_MAX;
    size_t offset_bits = std::bit_width(clamp_at_most(behind, WINDOW) - 1);
    if (reader.left() < offset_bits)
      return SIZE_MAX;
    size_t distance = reader.get(offset_bits) + 1;
    if (distance > behind)
      return SIZE_MAX;
    uint8_t zeros = 0;
    for (;;) {
      if (reader.left() == 0 || zeros > MAX_GAMMA_ZEROS)
        return SIZE_MAX;
      if (reader.get(1) != 0)
        break;
      zeros++;
    }
    if (reader.left() < zeros)
      return SIZE_MAX;
    size_t count = ((size_t{1} << zeros) | reader.get(zeros)) + 1;
    if (count > capacity - length)
      return SIZE_MAX;
    // One byte at a time, as the match may overlap the bytes it is writing
    for (; count != 0; count--, length++)
      out[length] = this->byte_at_(out, this->dictionary_size_ + length - distance);
  }
  return length;
}

}  // namespace esphome::lzss
