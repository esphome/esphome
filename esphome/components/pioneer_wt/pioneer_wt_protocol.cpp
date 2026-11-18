#include "pioneer_wt_protocol.h"

#include <cstring>
#include <iterator>

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

namespace esphome::pioneer_wt {

uint16_t frame_crc(const uint8_t *frame, size_t len) {
  // CRC-16/XMODEM over the whole frame except its own two bytes at offset 8.
  const uint16_t head = crc16be(frame, 8);
  return crc16be(frame + HEADER_SIZE, len - HEADER_SIZE, head);
}

size_t FrameReader::feed(uint8_t byte) {
  if (this->pos_ == 0 && byte != 0xA5)
    return 0;
  if (this->pos_ == 1 && byte != 0x01) {
    this->pos_ = byte == 0xA5 ? 1 : 0;
    return 0;
  }
  this->buf_[this->pos_++] = byte;
  if (this->pos_ < 8)
    return 0;
  const size_t len = this->buf_[7];
  if (len < HEADER_SIZE || len > MAX_FRAME_SIZE) {
    this->pos_ = 0;
    return 0;
  }
  if (this->pos_ < len)
    return 0;
  this->pos_ = 0;
  if (frame_crc(this->buf_.data(), len) != encode_uint16(this->buf_[8], this->buf_[9])) {
    this->checksum_error_ = true;
    return 0;
  }
  return len;
}

// The reader must know how every attribute is stored to step over the ones it does not use.
// The IDs are grouped by storage.
static const uint16_t ONE_BYTE_IDS[] PROGMEM = {
    0x0001, 0x0005, 0x000C, 0x000D, 0x000E, 0x0011, 0x0012, 0x0013, 0x0017, 0x001E, 0x0022, 0x0025, 0x0026, 0x0027,
    0x002D, 0x0035, 0x005E, 0x0073, 0x0074, 0x00A4, 0x00C9, 0x00DF, 0x0148, 0x015E, 0x0173, 0x0224, 0x0225,
};
static const uint16_t FOUR_BYTE_IDS[] PROGMEM = {
    0x0002, 0x0003, 0x003D, 0x0041, 0x0042, 0x005C, 0x0060, 0x0064, 0x0065,
    0x0072, 0x0095, 0x00BD, 0x00BE, 0x00BF, 0x00C0, 0x0221, 0x0222, 0x0227,
};
// One count byte, then that many bytes
static const uint16_t COUNT_PREFIXED_IDS[] PROGMEM = {0x0038, 0x0039};
// Two length bytes, then that many bytes
static const uint16_t LENGTH_PREFIXED_IDS[] PROGMEM = {0x00FA, 0x00FB, 0x0147, 0x0223};

static bool contains(const uint16_t *ids, size_t count, uint16_t id) {
  for (size_t i = 0; i < count; i++) {
    if (progmem_read_uint16(&ids[i]) == id)
      return true;
  }
  return false;
}

bool AttributeReader::next() {
  if (this->pos_ + 2 > this->len_)
    return false;
  const uint16_t id = encode_uint16(this->data_[this->pos_], this->data_[this->pos_ + 1]);
  size_t start = this->pos_ + 2;
  size_t value_len;
  bool is_number = true;
  if (contains(ONE_BYTE_IDS, std::size(ONE_BYTE_IDS), id)) {
    value_len = 1;
  } else if (contains(FOUR_BYTE_IDS, std::size(FOUR_BYTE_IDS), id)) {
    value_len = 4;
  } else {
    is_number = false;
    size_t prefix;
    if (contains(COUNT_PREFIXED_IDS, std::size(COUNT_PREFIXED_IDS), id)) {
      prefix = 1;
    } else if (contains(LENGTH_PREFIXED_IDS, std::size(LENGTH_PREFIXED_IDS), id)) {
      prefix = 2;
    } else {
      this->id_ = id;
      this->unknown_ = true;
      return false;
    }
    if (start + prefix > this->len_)
      return false;
    value_len = prefix == 1 ? this->data_[start] : encode_uint16(this->data_[start], this->data_[start + 1]);
    start += prefix;
  }
  if (start + value_len > this->len_)
    return false;
  this->id_ = id;
  this->value_ = 0;
  if (is_number) {
    for (size_t i = 0; i < value_len; i++)
      this->value_ = (this->value_ << 8) | this->data_[start + i];
  }
  this->pos_ = start + value_len;
  return true;
}

size_t build_frame(uint8_t *out, size_t out_size, uint8_t flags, uint8_t type, uint8_t seq, uint8_t ack,
                   const uint8_t *payload, size_t len) {
  const size_t total = HEADER_SIZE + len;
  if (total > out_size || total > MAX_FRAME_SIZE)
    return 0;
  out[0] = 0xA5;
  out[1] = 0x01;
  out[2] = flags;
  out[3] = type;
  out[4] = seq;
  out[5] = ack;
  out[6] = 0x00;
  out[7] = total;
  std::memcpy(out + HEADER_SIZE, payload, len);
  put_be16(out + 8, frame_crc(out, total));
  return total;
}

size_t put_attr(uint8_t *buf, size_t pos, uint16_t id, uint32_t value, uint8_t size) {
  uint8_t *end = put_be16(buf + pos, id);
  for (int8_t i = size - 1; i >= 0; i--)
    *end++ = (value >> (8 * i)) & 0xFF;
  return end - buf;
}

}  // namespace esphome::pioneer_wt
