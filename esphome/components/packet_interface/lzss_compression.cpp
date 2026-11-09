#include "lzss_compression.h"
#ifdef USE_PACKET_INTERFACE_LZSS
#include <algorithm>
#include <bit>
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::packet_interface {

ESPHOME_LOG_TAG(TAG, "packet_interface.lzss");

// Gamma codes up to this many bits after the first, so lengths up to 65536
static constexpr uint8_t MAX_GAMMA_ZEROS = 15;
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
  BitReader(const PacketBuffer &data, size_t offset, size_t end) : data_(data), pos_(offset), end_(end) {}

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
  const PacketBuffer &data_;
  size_t pos_;
  size_t end_;
  uint8_t byte_{0};
  uint8_t used_{0};
};

}  // namespace

LzssCompression::LzssCompression(PacketInterface *lower, size_t max_packet_size, const uint8_t *dictionary,
                                 size_t dictionary_size, uint8_t check)
    : PacketInterface(max_packet_size != 0 ? max_packet_size
                                           : remaining_packet_size(lower->get_max_packet_size(), HEADER_SIZE)),
      lower_(lower),
      dictionary_(dictionary),
      dictionary_size_(dictionary_size),
      check_(check & HEADER_CHECK_MASK) {
  this->stack_on_(lower, CAPABILITY_COMPRESSION);
}

void LzssCompression::setup() {
  size_t lower_size = this->lower_->get_max_packet_size();
  if (this->max_packet_size_ == 0 || lower_size <= HEADER_SIZE) {
    this->mark_failed(LOG_STR("max_packet_size too small for the configured layers"));
    return;
  }
  RAMAllocator<uint8_t> allocator;
  this->tx_input_ = allocator.allocate(2 * this->max_packet_size_ + lower_size);
  if (this->tx_input_ == nullptr) {
    this->mark_failed(LOG_STR("Not enough memory for the packet buffers"));
    return;
  }
  this->rx_output_ = this->tx_input_ + this->max_packet_size_;
  this->tx_output_ = this->rx_output_ + this->max_packet_size_;
  RAMAllocator<uint16_t> chains;
  this->head_ = chains.allocate((size_t{1} << HASH_BITS) + WINDOW);
  if (this->head_ == nullptr) {
    this->tx_input_ = nullptr;
    this->mark_failed(LOG_STR("Not enough memory for the packet buffers"));
    return;
  }
  this->previous_ = this->head_ + (size_t{1} << HASH_BITS);
  this->lower_->add_packet_interface_listener(
      [this](const PacketBuffer &data, const PacketMetaData &meta_data) { this->receive_(data, meta_data); });
}

void LzssCompression::dump_config() {
  ESP_LOGCONFIG(TAG,
                "LZSS Compression on '%s':\n"
                "  Max Packet Size: %zu\n"
                "  Dictionary Size: %zu",
                this->name_, this->max_packet_size_, this->dictionary_size_);
}

uint8_t LzssCompression::byte_at_(const uint8_t *data, size_t pos) const {
  return pos < this->dictionary_size_ ? progmem_read_byte(this->dictionary_ + pos) : data[pos - this->dictionary_size_];
}

void LzssCompression::insert_(const uint8_t *in, size_t pos, size_t total) {
  if (pos + 1 >= total)
    return;
  size_t hash = this->hash_(in, pos);
  this->previous_[pos & (WINDOW - 1)] = this->head_[hash];
  this->head_[hash] = pos;
}

size_t LzssCompression::compress(const uint8_t *in, size_t size, uint8_t *out, size_t capacity) {
  BitWriter writer(out, capacity);
  size_t total = this->dictionary_size_ + size;
  std::fill_n(this->head_, size_t{1} << HASH_BITS, NO_POSITION);
  for (size_t pos = 0; pos < this->dictionary_size_; pos++)
    this->insert_(in, pos, total);
  size_t i = 0;
  while (i < size && !writer.is_full()) {
    size_t pos = this->dictionary_size_ + i;
    size_t longest = size - i;
    size_t best_length = 0;
    size_t best_offset = 0;
    if (longest >= 2) {
      // The chain runs from the nearest, so of two matches of the same length the nearer is kept
      size_t tries = MAX_CHAIN;
      for (uint16_t ref = this->head_[this->hash_(in, pos)];
           ref != NO_POSITION && pos - ref <= WINDOW && tries-- != 0;) {
        size_t length = 0;
        // A match may run on past pos, copying bytes the expander has just written
        while (length < longest && this->byte_at_(in, ref + length) == in[i + length])
          length++;
        if (length > best_length) {
          best_length = length;
          best_offset = pos - ref;
          if (length == longest)
            break;
        }
        // A slot reused by a later position ends the chain
        uint16_t next = this->previous_[ref & (WINDOW - 1)];
        if (next >= ref)
          break;
        ref = next;
      }
    }
    size_t step = 1;
    if (best_length >= 2 && 1 + std::bit_width(pos - 1) + gamma_bits(best_length - 1) < LITERAL_BITS * best_length) {
      writer.put(0, 1);
      writer.put(best_offset - 1, std::bit_width(pos - 1));
      writer.put_gamma(best_length - 1);
      step = best_length;
    } else {
      writer.put(1, 1);
      writer.put(in[i], 8);
    }
    for (size_t end = pos + step; pos != end; pos++)
      this->insert_(in, pos, total);
    i += step;
  }
  return writer.finish();
}

size_t LzssCompression::expand(const PacketBuffer &data, size_t offset, size_t size, uint8_t *out,
                               size_t capacity) const {
  BitReader reader(data, offset, size);
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
    size_t offset_bits = std::bit_width(behind - 1);
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

bool LzssCompression::send_to_interface(const PacketBuffer &data, const PacketMetaData &meta_data) {
  if (this->tx_input_ == nullptr)
    return false;
  size_t size = data.size();
  if (size > this->max_packet_size_) {
    ESP_LOGW(TAG, "%s: Packet of %zu bytes is larger than %zu", this->name_, size, this->max_packet_size_);
    return false;
  }
  const uint8_t *in = this->tx_input_;
  if (data.is_contiguous()) {
    in = data.as_span().data();
  } else {
    data.copy_to(this->tx_input_, size);
  }
  size_t room = this->lower_->get_max_packet_size() - HEADER_SIZE;
  // Compressed only if that makes it smaller
  size_t length = size > 1 ? this->compress(in, size, this->tx_output_ + HEADER_SIZE, std::min(room, size - 1)) : 0;
  if (length != 0) {
    ESP_LOGV(TAG, "%s: Packet of %zu bytes compressed to %zu", this->name_, size, length);
    this->tx_output_[0] = HEADER_COMPRESSED | this->check_;
    return this->lower_->send_to_interface(PacketBuffer(this->tx_output_, length + HEADER_SIZE), meta_data);
  }
  if (size > room) {
    ESP_LOGW(TAG, "%s: Packet of %zu bytes does not compress to %zu", this->name_, size, room);
    return false;
  }
  uint8_t header = this->check_;
  return this->lower_->send_to_interface(PacketBuffer(&header, HEADER_SIZE, &data, nullptr, 0), meta_data);
}

void LzssCompression::receive_(const PacketBuffer &data, const PacketMetaData &meta_data) {
  size_t size = data.size();
  if (size < HEADER_SIZE || (data[0] & HEADER_CHECK_MASK) != this->check_) {
    ESP_LOGV(TAG, "%s: Packet of %zu bytes with another dictionary dropped", this->name_, size);
    this->note_received_(false);
    return;
  }
  if ((data[0] & HEADER_COMPRESSED) == 0) {
    if (size - HEADER_SIZE > this->max_packet_size_) {
      ESP_LOGV(TAG, "%s: Packet of %zu bytes is larger than %zu", this->name_, size, this->max_packet_size_);
      this->note_received_(false);
      return;
    }
    this->note_received_(true);
    if (data.is_contiguous()) {
      this->on_receive_from_interface_(PacketBuffer(data.as_span().data() + HEADER_SIZE, size - HEADER_SIZE),
                                       meta_data);
    } else {
      data.copy_to(this->rx_output_, size - HEADER_SIZE, HEADER_SIZE);
      this->on_receive_from_interface_(PacketBuffer(this->rx_output_, size - HEADER_SIZE), meta_data);
    }
    return;
  }
  size_t length = this->expand(data, HEADER_SIZE, size, this->rx_output_, this->max_packet_size_);
  if (length == SIZE_MAX) {
    ESP_LOGV(TAG, "%s: Malformed packet of %zu bytes dropped", this->name_, size);
    this->note_received_(false);
    return;
  }
  this->note_received_(true);
  this->on_receive_from_interface_(PacketBuffer(this->rx_output_, length), meta_data);
}

}  // namespace esphome::packet_interface
#endif
