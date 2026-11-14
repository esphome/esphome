#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "esphome/core/helpers.h"

namespace esphome::esp32_ble {

/// Parse "xx:xx:xx:xx:xx:xx" as Bluedroid's string_to_bdaddr does: one or two hex digits per byte, ':' between
/// bytes, anything after the sixth byte ignored. addr (6 bytes) is written only on success.
inline bool parse_bdaddr(const char *str, uint8_t *addr) {
  uint8_t out[6];
  for (size_t i = 0; i < sizeof(out); i++) {
    uint8_t value = parse_hex_char(*str++);
    if (value == INVALID_HEX_CHAR)
      return false;
    uint8_t low = parse_hex_char(*str);
    if (low != INVALID_HEX_CHAR) {
      value = (value << 4) | low;
      str++;
    }
    if (i < sizeof(out) - 1 && *str++ != ':')
      return false;
    out[i] = value;
  }
  memcpy(addr, out, sizeof(out));
  return true;
}

/// Decode a stored hex string as Bluedroid's btc_config_get_bin does: even length, at most *length bytes, hex
/// digits only. On success *length is the decoded size; on failure value and *length are untouched.
inline bool parse_config_bin(const char *str, size_t len, uint8_t *value, size_t *length) {
  if (len % 2 != 0 || len / 2 > *length)
    return false;
  for (size_t i = 0; i < len; i++) {
    if (parse_hex_char(str[i]) == INVALID_HEX_CHAR)
      return false;
  }
  parse_hex(str, len, value, len / 2);
  *length = len / 2;
  return true;
}

// Covers every btc_config_get_bin caller in Bluetooth LE builds (bonding keys are at most 28 bytes)
static constexpr size_t CONFIG_BIN_STACK_BYTES = 32;

/// btc_config_get_bin on top of btc_config_get_str, passed as get_str, which strlcpy's the stored string.
template<typename GetStr>
bool config_get_bin(GetStr get_str, const char *section, const char *key, uint8_t *value, size_t *length) {
  // A stored string too long for *length is truncated to an odd length, which parse_config_bin rejects
  size_t size = 2 * *length + 2;
  SmallBufferWithHeapFallback<2 * CONFIG_BIN_STACK_BYTES + 2, char> buf(size);
  int size_bytes = static_cast<int>(size);
  if (!get_str(section, key, buf.get(), &size_bytes))
    return false;
  return parse_config_bin(buf.get(), static_cast<size_t>(size_bytes) - 1, value, length);
}

}  // namespace esphome::esp32_ble
