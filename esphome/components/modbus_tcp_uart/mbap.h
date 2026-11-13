#pragma once

#include "esphome/core/helpers.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace esphome::modbus_tcp_uart {

static constexpr size_t MBAP_HEADER_SIZE = 7;
static constexpr size_t MBAP_MAX_LENGTH = 254;

struct Mbap {
  const uint8_t *pdu;
  size_t pdu_len;
  uint16_t txn;
  uint8_t unit;
};

enum class MbapTake : uint8_t {
  NEED_MORE,
  BAD,
  FRAME,
};

inline MbapTake take_mbap(const uint8_t *buf, size_t len, Mbap *out, size_t *used) {
  *used = 0;
  if (len < MBAP_HEADER_SIZE) {
    return MbapTake::NEED_MORE;
  }
  uint16_t proto = encode_uint16(buf[2], buf[3]);
  uint16_t length = encode_uint16(buf[4], buf[5]);
  if (proto != 0 || length < 2 || length > MBAP_MAX_LENGTH) {
    *used = 1;
    return MbapTake::BAD;
  }
  if (len < 6u + length) {
    return MbapTake::NEED_MORE;
  }
  out->txn = encode_uint16(buf[0], buf[1]);
  out->unit = buf[6];
  out->pdu = buf + MBAP_HEADER_SIZE;
  out->pdu_len = length - 1;
  *used = 6u + length;
  return MbapTake::FRAME;
}

/// Size of the frame a header announces, or 0 when its length field is not usable.
inline size_t mbap_announced_size(const uint8_t *buf, size_t len) {
  if (len < MBAP_HEADER_SIZE) {
    return 0;
  }
  uint16_t length = encode_uint16(buf[4], buf[5]);
  if (length < 2 || length > MBAP_MAX_LENGTH) {
    return 0;
  }
  return 6u + length;
}

inline size_t write_mbap(uint8_t *dst, size_t cap, uint16_t txn, uint8_t unit, const uint8_t *pdu, size_t pdu_len) {
  if (pdu_len == 0 || pdu_len > MBAP_MAX_LENGTH - 1) {
    return 0;
  }
  size_t n = MBAP_HEADER_SIZE + pdu_len;
  if (cap < n) {
    return 0;
  }
  uint16_t length = static_cast<uint16_t>(pdu_len + 1);
  dst[0] = txn >> 8;
  dst[1] = txn & 0xFF;
  dst[2] = 0;
  dst[3] = 0;
  dst[4] = length >> 8;
  dst[5] = length & 0xFF;
  dst[6] = unit;
  std::memcpy(dst + MBAP_HEADER_SIZE, pdu, pdu_len);
  return n;
}

/// Writes unit, PDU and CRC as an RTU frame. dst holds at least pdu_len + 3 bytes. Returns the frame length.
inline size_t write_rtu(uint8_t *dst, uint8_t unit, const uint8_t *pdu, size_t pdu_len) {
  dst[0] = unit;
  std::memcpy(dst + 1, pdu, pdu_len);
  uint16_t crc = crc16(dst, static_cast<uint16_t>(pdu_len + 1));
  dst[pdu_len + 1] = crc & 0xFF;
  dst[pdu_len + 2] = crc >> 8;
  return pdu_len + 3;
}

// Unit, PDU of at most 253 bytes, and CRC.
static constexpr size_t RTU_MAX_SIZE = 256;
// An exception reply sets this bit in its function code.
static constexpr uint8_t EXCEPTION_FLAG = 0x80;

inline bool rtu_crc_ok(const uint8_t *frame, size_t len) {
  return len >= 4 && len <= RTU_MAX_SIZE && crc16(frame, static_cast<uint16_t>(len)) == 0;
}

enum class RtuTake : uint8_t {
  NEED_MORE,
  BAD,
  FRAME,
};

/// Length of the RTU frame at the start of buf as its function code gives it, or 0 for a code without a known length.
/// replies: buf holds what a server sends. buf holds at least 2 bytes. A link without a modbus hub needs this copy of
/// the modbus component's server_frame_length() and client_frame_length(); only the Read FIFO Queue reply differs.
inline size_t rtu_frame_length(const uint8_t *buf, size_t len, bool replies) {
  static constexpr uint8_t MAX_READ_BYTES = 250;         // 2000 coils or 125 registers
  static constexpr uint8_t MAX_WRITE_BYTES = 246;        // 1968 coils or 123 registers
  static constexpr uint8_t MAX_FILE_RECORD_BYTES = 251;  // a 253-byte PDU less function code and byte count
  static constexpr uint8_t MAX_READ_WRITE_BYTES = 242;   // 121 registers to write
  static constexpr uint8_t MAX_FIFO_BYTES = 64;          // FIFO count and 31 values
  // A byte count is 0 until it arrives, and capped at these limits.
  const auto count = [buf, len](size_t at, uint8_t max) -> size_t { return len > at ? std::min(buf[at], max) : 0; };
  // Every frame has unit, function code and CRC: 4 bytes.
  if ((buf[1] & EXCEPTION_FLAG) != 0) {
    return 5;  // plus the exception code
  }
  switch (buf[1]) {
    case 0x01:  // reads: address and quantity; the reply has a byte count and data
    case 0x02:
    case 0x03:
    case 0x04:
      return replies ? 5 + count(2, MAX_READ_BYTES) : 8;
    case 0x05:  // single writes: address and value, echoed
    case 0x06:
      return 8;
    case 0x0F:  // multiple writes: address, quantity, byte count and data; the reply has address and quantity
    case 0x10:
      return replies ? 8 : 9 + count(6, MAX_WRITE_BYTES);
    case 0x14:  // file records: byte count and data
    case 0x15:
      return 5 + count(2, MAX_FILE_RECORD_BYTES);
    case 0x16:  // mask write: address, AND mask and OR mask, echoed
      return 10;
    case 0x17:  // read/write multiple: two addresses and quantities, byte count and data; the reply as a read
      return replies ? 5 + count(2, MAX_READ_BYTES) : 13 + count(10, MAX_READ_WRITE_BYTES);
    case 0x18:  // read FIFO queue: the request has its pointer address; the reply has a 2-byte byte count, at most 64
      return replies ? 6 + count(3, MAX_FIFO_BYTES) : 6;
    default:
      return 0;
  }
}

/// Finds the RTU frame at the start of buf: by the length its function code gives, else at the last length up to len
/// (at most RTU_MAX_SIZE) whose CRC checks. A 0x00 right behind a frame keeps the CRC at 0, so it is taken into it.
/// replies: buf holds what a server sends; else what a client sends. *frame_len is set for FRAME.
inline RtuTake take_rtu(const uint8_t *buf, size_t len, bool replies, size_t *frame_len) {
  *frame_len = 0;
  if (len < 2) {
    return RtuTake::NEED_MORE;
  }
  const size_t want = rtu_frame_length(buf, len, replies);
  if (want != 0) {
    if (len < want) {
      return RtuTake::NEED_MORE;
    }
    if (!rtu_crc_ok(buf, want)) {
      return RtuTake::BAD;
    }
    *frame_len = want;
    return RtuTake::FRAME;
  }
  uint16_t crc = 0xFFFF;
  size_t last_match = 0;
  for (size_t n = 1; n <= len && n <= RTU_MAX_SIZE; n++) {
    crc = crc16(buf + n - 1, 1, crc);
    if (n >= 4 && crc == 0) {
      last_match = n;
    }
  }
  if (last_match != 0) {
    *frame_len = last_match;
    return RtuTake::FRAME;
  }
  return len >= RTU_MAX_SIZE ? RtuTake::BAD : RtuTake::NEED_MORE;
}

}  // namespace esphome::modbus_tcp_uart
