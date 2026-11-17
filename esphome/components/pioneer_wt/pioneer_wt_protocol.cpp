#include "pioneer_wt_protocol.h"

#include <cstring>

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
  const uint16_t expected = (static_cast<uint16_t>(this->buf_[8]) << 8) | this->buf_[9];
  if (frame_crc(this->buf_.data(), len) != expected)
    return 0;
  return len;
}

static int value_size(uint16_t id) {
  switch (id) {
    case 0x0002:
    case 0x0003:
    case 0x003D:
    case 0x0041:
    case 0x0042:
    case 0x005C:
    case 0x0060:
    case 0x0064:
    case 0x0065:
    case 0x0072:
    case 0x0095:
    case 0x00BD:
    case 0x00BE:
    case 0x00BF:
    case 0x00C0:
    case 0x0221:
    case 0x0222:
    case 0x0227:
      return 4;
    case 0x0001:
    case 0x0005:
    case 0x000C:
    case 0x000D:
    case 0x000E:
    case 0x0011:
    case 0x0012:
    case 0x0013:
    case 0x0017:
    case 0x001E:
    case 0x0022:
    case 0x0025:
    case 0x0026:
    case 0x0027:
    case 0x002D:
    case 0x0035:
    case 0x005E:
    case 0x0073:
    case 0x0074:
    case 0x00A4:
    case 0x00C9:
    case 0x00DF:
    case 0x0148:
    case 0x015E:
    case 0x0173:
    case 0x0224:
    case 0x0225:
      return 1;
    case 0x0147:
    case 0x0223:
    case 0x00FA:
    case 0x00FB:
      return -2;  // two length bytes, then that many bytes
    case 0x0038:
    case 0x0039:
      return -1;  // one count byte, then that many bytes
    default:
      return 0;
  }
}

bool AttributeReader::next() {
  if (this->pos_ + 2 > this->len_)
    return false;
  const uint16_t id = (static_cast<uint16_t>(this->data_[this->pos_]) << 8) | this->data_[this->pos_ + 1];
  const int size = value_size(id);
  size_t start = this->pos_ + 2;
  size_t value_len;
  if (size > 0) {
    value_len = size;
  } else if (size == -1 && start < this->len_) {
    value_len = this->data_[start];
    start += 1;
  } else if (size == -2 && start + 1 < this->len_) {
    value_len = (static_cast<size_t>(this->data_[start]) << 8) | this->data_[start + 1];
    start += 2;
  } else {
    this->id_ = id;
    this->unknown_ = true;
    return false;
  }
  if (start + value_len > this->len_)
    return false;
  this->id_ = id;
  this->value_ = 0;
  if (size > 0) {
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
  const uint16_t crc = frame_crc(out, total);
  out[8] = crc >> 8;
  out[9] = crc & 0xFF;
  return total;
}

size_t put_attr(uint8_t *buf, size_t pos, uint16_t id, uint32_t value, uint8_t size) {
  buf[pos++] = id >> 8;
  buf[pos++] = id & 0xFF;
  for (int8_t i = size - 1; i >= 0; i--)
    buf[pos++] = (value >> (8 * i)) & 0xFF;
  return pos;
}

}  // namespace esphome::pioneer_wt
