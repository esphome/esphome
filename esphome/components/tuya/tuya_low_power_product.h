#pragma once

// Strict, bounded extraction of the product metadata used by the original
// low-power MCU initialization. No JSON library or platform dependency.
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace esphome::tuya {

struct TuyaLowPowerProductInfo {
  // Product fields are at most 48 characters; one more for the terminator.
  static constexpr size_t FIELD_SIZE = 49;

  std::array<char, FIELD_SIZE> pid{};
  std::array<char, FIELD_SIZE> version{};
  int wake_gpio{-1};      // wk: logical WXU MCU-to-WXU wake source
  int mcu_wake_gpio{-1};  // mwk: vendor-encoded WXU-to-MCU wake line
  int sleep_mode{-1};     // sm: 0 hibernation, 1 standby on the MCU's CMD38, 2 standby offered by the module (0x91)
  // mwkpt: length of the WXU-to-MCU wake pulse. The original WXU firmware uses 50 ms when the MCU
  // does not report it (the TRV16 does not) and limits a reported value to 5..200 ms.
  uint32_t mcu_wake_pulse_ms{50};
  // mwkp: active level of the wake line to the MCU; the original firmware defaults to low.
  bool mcu_wake_active_high{false};
  bool parsed{false};

  /// Index of the value after `"key":` in s[0..len), or len if the key is missing or malformed.
  static size_t find_key(const char *s, size_t len, const char *key) {
    const size_t key_len = std::strlen(key);
    for (size_t pos = 0; pos + key_len + 2 <= len; ++pos) {
      if (s[pos] != '"' || s[pos + key_len + 1] != '"' || std::memcmp(s + pos + 1, key, key_len) != 0)
        continue;
      size_t i = pos + key_len + 2;
      while (i < len && (s[i] == ' ' || s[i] == '\t'))
        ++i;
      if (i >= len || s[i] != ':')
        return len;
      ++i;
      while (i < len && (s[i] == ' ' || s[i] == '\t'))
        ++i;
      return i;
    }
    return len;
  }

  static bool get_string(const char *s, size_t len, const char *key, std::array<char, FIELD_SIZE> &value) {
    size_t i = find_key(s, len, key);
    if (i >= len || s[i] != '"')
      return false;
    ++i;
    const size_t begin = i;
    while (i < len && s[i] != '"') {
      if (s[i] == '\\' || static_cast<unsigned char>(s[i]) < 32)
        return false;  // No escape sequences in the product fields.
      ++i;
    }
    if (i >= len || i == begin || i - begin >= FIELD_SIZE)
      return false;
    std::memcpy(value.data(), s + begin, i - begin);
    value[i - begin] = '\0';
    return true;
  }

  static bool get_integer(const char *s, size_t len, const char *key, int &value) {
    size_t i = find_key(s, len, key);
    if (i >= len || !std::isdigit(static_cast<unsigned char>(s[i])))
      return false;
    unsigned n = 0;
    while (i < len && std::isdigit(static_cast<unsigned char>(s[i]))) {
      n = 10 * n + static_cast<unsigned>(s[i++] - '0');
      if (n > 255)
        return false;
    }
    value = static_cast<int>(n);
    return true;
  }

  bool parse(const uint8_t *data, size_t len) {
    *this = {};
    if (data == nullptr || len < 12 || len > 512)
      return false;
    const char *s = reinterpret_cast<const char *>(data);
    if (s[0] != '{' || s[len - 1] != '}')
      return false;
    // Like the original WXU, only p and v are required. MCU firmware versions differ in the rest; without
    // wk, mwk or sm the fields stay -1, and standby is refused because it needs sm=1 and a matching wk.
    parsed = get_string(s, len, "p", pid) && get_string(s, len, "v", version);
    if (!parsed) {
      pid[0] = '\0';
      version[0] = '\0';
      return false;
    }
    (void) get_integer(s, len, "wk", wake_gpio);
    (void) get_integer(s, len, "mwk", mcu_wake_gpio);
    if (get_integer(s, len, "sm", sleep_mode) && sleep_mode > 2)
      sleep_mode = -1;
    int level = 0;
    if (get_integer(s, len, "mwkp", level))
      mcu_wake_active_high = level == 1;
    int pulse_ms = 0;
    if (get_integer(s, len, "mwkpt", pulse_ms))
      mcu_wake_pulse_ms = pulse_ms < 5 ? 5 : (pulse_ms > 200 ? 200 : static_cast<uint32_t>(pulse_ms));
    return true;
  }
};

}  // namespace esphome::tuya
