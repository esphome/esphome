#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "esphome/core/helpers.h"

namespace esphome::esp32_ble {

static constexpr uint8_t ATT_FIND_INFO_RSP = 0x05;
static constexpr uint8_t ATT_FIND_BY_TYPE_VALUE_RSP = 0x07;
static constexpr uint8_t ATT_READ_BY_TYPE_RSP = 0x09;
static constexpr uint8_t ATT_READ_BY_GRP_TYPE_RSP = 0x11;
static constexpr size_t ATT_MAX_MTU = 517;

/// Sort records of `size` bytes that each start with a little-endian handle, in place; a partial tail is left alone.
inline void sort_records_by_handle(uint8_t *records, size_t len, size_t size) {
  const size_t count = len / size;
  auto handle = [records, size](size_t i) { return encode_uint16(records[i * size + 1], records[i * size]); };
  for (size_t i = 1; i < count; i++) {
    const uint16_t current = handle(i);
    size_t j = i;
    while (j > 0 && handle(j - 1) > current)
      j--;
    if (j != i)
      std::rotate(records + j * size, records + i * size, records + (i + 1) * size);
  }
}

/// Sort the records of an ATT discovery response by handle, in place; `data` is the PDU after the opcode.
/// A body longer than any ATT_MTU allows is left for Bluedroid to reject, so a peer cannot make the sort costly.
inline void sort_att_response_by_handle(uint8_t op_code, uint8_t *data, size_t len) {
  if (len < 1 || len >= ATT_MAX_MTU)
    return;
  switch (op_code) {
    case ATT_READ_BY_TYPE_RSP:
    case ATT_READ_BY_GRP_TYPE_RSP:
      // Length byte, then records of that length
      if (data[0] >= 2)
        sort_records_by_handle(data + 1, len - 1, data[0]);
      break;
    case ATT_FIND_INFO_RSP:
      // Format byte: handle and 16-bit UUID, or handle and 128-bit UUID
      if (data[0] == 1 || data[0] == 2)
        sort_records_by_handle(data + 1, len - 1, data[0] == 1 ? 4 : 18);
      break;
    case ATT_FIND_BY_TYPE_VALUE_RSP:
      // Found handle and group end handle, no header
      sort_records_by_handle(data, len, 4);
      break;
    default:
      break;
  }
}

}  // namespace esphome::esp32_ble
