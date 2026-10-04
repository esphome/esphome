#pragma once

#include "esphome/core/helpers.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace esphome::modbus_tcp {

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

inline bool rtu_crc_ok(const uint8_t *frame, size_t len) {
  return len >= 4 && len <= 256 && crc16(frame, static_cast<uint16_t>(len)) == 0;
}

}  // namespace esphome::modbus_tcp
