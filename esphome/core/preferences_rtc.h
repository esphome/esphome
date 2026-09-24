#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace esphome {

// Shared storage format for word-addressable preference backends.
//
// Several platforms persist preferences as a buffer of 32-bit words followed by a
// single checksum word, seeded with the preference's `type` (its hashed key). This
// format is used for RTC user memory (ESP8266, ESP32) and for the ESP8266
// flash-emulation buffer. The helpers here are platform independent; each backend
// supplies its own word read/write primitives and offset allocation.

/// Round a byte count up to whole 32-bit words.
inline size_t rtc_pref_bytes_to_words(size_t bytes) { return (bytes + 3) / 4; }

/// Compute the integrity checksum over [first, last), seeded with `type`.
/// Iterates over 32-bit words; the result is stored as the trailing word of a record.
/// (Not a true CRC -- it XORs each word after a Fibonacci-hash multiply -- but the
/// algorithm is kept as-is for compatibility with records written by old firmware.)
template<class It> uint32_t rtc_pref_calculate_checksum(It first, It last, uint32_t type) {
  uint32_t checksum = type;
  while (first != last) {
    // UINT32_C keeps the multiply wrapping at 32 bits regardless of the width of
    // unsigned long, so 64-bit host builds compute the same value as the devices.
    checksum ^= (*first++ * UINT32_C(2654435769)) >> 1;
  }
  return checksum;
}

/// Encode `len` data bytes into `buffer` (length_words data words + 1 trailing checksum word).
/// `buffer` must have capacity for at least `length_words + 1` words. Trailing padding in
/// the final data word is zeroed so the checksum is deterministic.
inline void rtc_pref_encode(uint32_t *buffer, uint32_t type, uint8_t length_words, const uint8_t *data, size_t len) {
  memset(buffer, 0, (static_cast<size_t>(length_words) + 1) * sizeof(uint32_t));
  memcpy(buffer, data, len);
  buffer[length_words] = rtc_pref_calculate_checksum(buffer, buffer + length_words, type);
}

/// Verify the checksum of a record held in `buffer` (length_words data words + 1 checksum
/// word) and, on success, copy `len` bytes out to `data`. Returns false on checksum mismatch
/// (e.g. the record was never written or RTC memory holds power-on garbage).
inline bool rtc_pref_decode(const uint32_t *buffer, uint32_t type, uint8_t length_words, uint8_t *data, size_t len) {
  if (buffer[length_words] != rtc_pref_calculate_checksum(buffer, buffer + length_words, type)) {
    return false;
  }
  memcpy(data, buffer, len);
  return true;
}

}  // namespace esphome
