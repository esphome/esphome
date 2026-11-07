#pragma once

#include "esphome/core/defines.h"
#ifdef USE_CRC32

#include <cstddef>
#include <cstdint>

namespace esphome::crc32 {

/** Compute a standard CRC-32 (IEEE 802.3, reflected, as used by zlib,
 * Ethernet, PNG and the Tuya protocols).
 *
 * Pass 0 as the initial value; to checksum data in chunks, pass the previous
 * return value as the crc of the next call.
 */
uint32_t crc32(uint32_t crc, const uint8_t *data, size_t len);

}  // namespace esphome::crc32

#endif
