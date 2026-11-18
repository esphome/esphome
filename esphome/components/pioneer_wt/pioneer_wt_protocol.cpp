#include "pioneer_wt_protocol.h"

#include <cstring>

#include "esphome/core/hal.h"

namespace esphome::pioneer_wt {

uint16_t frame_crc(const uint8_t *frame, size_t len) {
  const uint16_t head = crc16be(frame, 8);
  return crc16be(frame + HEADER_SIZE, len - HEADER_SIZE, head);
}

size_t PacketReader::feed(uint8_t byte) {
  if (this->pos_ == 0 && byte != 0xA5)
    return 0;
  if (this->pos_ == 1 && byte != 0x01) {
    this->pos_ = byte == 0xA5 ? 1 : 0;
    return 0;
  }
  this->buf_[this->pos_++] = byte;
  if (this->pos_ < 8)
    return 0;
  const uint8_t len = this->buf_[7];
  if (len < HEADER_SIZE) {
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

enum class AttrKind : uint8_t {
  ATTR_KIND_UNKNOWN,
  ATTR_KIND_ONE_BYTE,
  ATTR_KIND_FOUR_BYTES,
  // One count byte, then that many bytes
  ATTR_KIND_COUNT_PREFIXED,
  // Two length bytes, then that many bytes
  ATTR_KIND_LENGTH_PREFIXED,
};

// The reader must know how every attribute is stored to step over the ones it does not use.
// The IDs are grouped by storage.
static constexpr uint16_t ONE_BYTE_IDS[] PROGMEM = {
    0x0001, 0x0005, 0x000C, 0x000D, 0x000E, 0x0011, 0x0012, 0x0013, 0x0017, 0x001E, 0x0022, 0x0025, 0x0026, 0x0027,
    0x002D, 0x0035, 0x005E, 0x0073, 0x0074, 0x00A4, 0x00C9, 0x00DF, 0x0148, 0x015E, 0x0173, 0x0224, 0x0225,
};
static constexpr uint16_t FOUR_BYTE_IDS[] PROGMEM = {
    0x0002, 0x0003, 0x003D, 0x0041, 0x0042, 0x005C, 0x0060, 0x0064, 0x0065,
    0x0072, 0x0095, 0x00BD, 0x00BE, 0x00BF, 0x00C0, 0x0221, 0x0222, 0x0227,
};
static constexpr uint16_t COUNT_PREFIXED_IDS[] PROGMEM = {0x0038, 0x0039};
static constexpr uint16_t LENGTH_PREFIXED_IDS[] PROGMEM = {0x00FA, 0x00FB, 0x0147, 0x0223};

template<size_t N> static bool contains(const uint16_t (&ids)[N], uint16_t id) {
  for (const uint16_t &entry : ids) {
    if (progmem_read_uint16(&entry) == id)
      return true;
  }
  return false;
}

static AttrKind attr_kind(uint16_t id) {
  if (contains(ONE_BYTE_IDS, id))
    return AttrKind::ATTR_KIND_ONE_BYTE;
  if (contains(FOUR_BYTE_IDS, id))
    return AttrKind::ATTR_KIND_FOUR_BYTES;
  if (contains(COUNT_PREFIXED_IDS, id))
    return AttrKind::ATTR_KIND_COUNT_PREFIXED;
  if (contains(LENGTH_PREFIXED_IDS, id))
    return AttrKind::ATTR_KIND_LENGTH_PREFIXED;
  return AttrKind::ATTR_KIND_UNKNOWN;
}

bool AttributeReader::next() {
  if (this->pos_ + 2 > this->len_)
    return false;
  const uint8_t *data = this->data_;
  const uint16_t id = encode_uint16(data[this->pos_], data[this->pos_ + 1]);
  size_t start = this->pos_ + 2;
  size_t value_len;
  const AttrKind kind = attr_kind(id);
  switch (kind) {
    case AttrKind::ATTR_KIND_ONE_BYTE:
      value_len = 1;
      break;
    case AttrKind::ATTR_KIND_FOUR_BYTES:
      value_len = 4;
      break;
    case AttrKind::ATTR_KIND_COUNT_PREFIXED:
      if (start + 1 > this->len_)
        return false;
      value_len = data[start++];
      break;
    case AttrKind::ATTR_KIND_LENGTH_PREFIXED:
      if (start + 2 > this->len_)
        return false;
      value_len = encode_uint16(data[start], data[start + 1]);
      start += 2;
      break;
    default:
      this->id_ = id;
      this->unknown_ = true;
      return false;
  }
  if (start + value_len > this->len_)
    return false;
  this->id_ = id;
  const uint8_t *v = data + start;
  if (kind == AttrKind::ATTR_KIND_ONE_BYTE) {
    this->value_ = v[0];
  } else if (kind == AttrKind::ATTR_KIND_FOUR_BYTES) {
    this->value_ = encode_uint32(v[0], v[1], v[2], v[3]);
  } else {
    this->value_ = 0;
  }
  this->pos_ = start + value_len;
  return true;
}

size_t build_frame(uint8_t *out, uint8_t flags, uint8_t type, uint8_t seq, uint8_t ack, uint8_t class_1,
                   uint8_t class_2, const uint8_t *body, size_t len) {
  const size_t total = HEADER_SIZE + 2 + len;
  out[0] = 0xA5;
  out[1] = 0x01;
  out[2] = flags;
  out[3] = type;
  out[4] = seq;
  out[5] = ack;
  out[6] = 0x00;
  out[7] = total;
  out[HEADER_SIZE] = class_1;
  out[HEADER_SIZE + 1] = class_2;
  if (len)
    std::memcpy(out + HEADER_SIZE + 2, body, len);
  put_be16(out + 8, frame_crc(out, total));
  return total;
}

}  // namespace esphome::pioneer_wt
