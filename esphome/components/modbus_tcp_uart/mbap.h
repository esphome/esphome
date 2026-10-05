#pragma once

#include "esphome/components/modbus/modbus_helpers.h"
#include "esphome/core/helpers.h"

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
  uint16_t proto = (static_cast<uint16_t>(buf[2]) << 8) | buf[3];
  uint16_t length = (static_cast<uint16_t>(buf[4]) << 8) | buf[5];
  if (proto != 0 || length < 2 || length > MBAP_MAX_LENGTH) {
    *used = 1;
    return MbapTake::BAD;
  }
  if (len < 6u + length) {
    return MbapTake::NEED_MORE;
  }
  out->txn = (static_cast<uint16_t>(buf[0]) << 8) | buf[1];
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
  uint16_t length = (static_cast<uint16_t>(buf[4]) << 8) | buf[5];
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

// Unit, PDU and CRC.
static constexpr size_t RTU_MAX_SIZE = modbus::MAX_PDU_SIZE + 3;

inline bool rtu_crc_ok(const uint8_t *frame, size_t len) {
  return len >= 4 && len <= RTU_MAX_SIZE && crc16(frame, static_cast<uint16_t>(len)) == 0;
}

enum class RtuTake : uint8_t {
  NEED_MORE,
  BAD,
  FRAME,
};

/// Finds the RTU frame at the start of buf: by the length its function code gives, else at the first CRC match.
/// replies: buf holds what a server sends; else what a client sends. *frame_len is set for FRAME.
inline RtuTake take_rtu(const uint8_t *buf, size_t len, bool replies, size_t *frame_len) {
  *frame_len = 0;
  if (len < 2) {
    return RtuTake::NEED_MORE;
  }
  if (!modbus::helpers::is_function_code_unknown_length(buf[1])) {
    const size_t want =
        replies ? modbus::helpers::server_frame_length(buf, len) : modbus::helpers::client_frame_length(buf, len);
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
  for (size_t n = 1; n <= len && n <= RTU_MAX_SIZE; n++) {
    crc = crc16(buf + n - 1, 1, crc);
    if (n >= 4 && crc == 0) {
      *frame_len = n;
      return RtuTake::FRAME;
    }
  }
  return len >= RTU_MAX_SIZE ? RtuTake::BAD : RtuTake::NEED_MORE;
}

}  // namespace esphome::modbus_tcp_uart
