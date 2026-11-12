#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "esphome/core/helpers.h"

namespace esphome::esp32_ble {

/// Sort the records of an ATT Read By Type or Read By Group Type response by their leading handle, in place.
/// `data` is the PDU after the opcode: one length byte, then records of that length; a partial tail is left alone.
inline void sort_att_records_by_handle(uint8_t *data, size_t len) {
  if (len < 1 || data[0] < 2)
    return;
  const size_t size = data[0];
  uint8_t *records = data + 1;
  const size_t count = (len - 1) / size;
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

}  // namespace esphome::esp32_ble
