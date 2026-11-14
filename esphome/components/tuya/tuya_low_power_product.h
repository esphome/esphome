#pragma once

// Strict, bounded extraction of the product metadata used by the original
// low-power MCU initialization. No JSON library or platform dependency.
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>

namespace esphome::tuya {

struct TuyaLowPowerProductInfo {
  std::string pid;
  std::string version;
  int wake_gpio{-1};      // wk: logical WXU MCU-to-WXU wake source
  int mcu_wake_gpio{-1};  // mwk: vendor-encoded WXU-to-MCU wake line
  int sleep_mode{-1};     // sm: 0 hibernation, 1 standby on the MCU's CMD38, 2 standby offered by the module (0x91)
  // mwkpt: length of the WXU-to-MCU wake pulse. The original WXU firmware uses 50 ms when the MCU
  // does not report it (the TRV16 does not) and limits a reported value to 5..200 ms.
  uint32_t mcu_wake_pulse_ms{50};
  // mwkp: active level of the wake line to the MCU; the original firmware defaults to low.
  bool mcu_wake_active_high{false};
  bool parsed{false};

  static bool find_key(const std::string &s, const char *key, size_t &value) {
    const std::string needle = std::string("\"") + key + "\"";
    const size_t pos = s.find(needle);
    if (pos == std::string::npos)
      return false;
    size_t i = pos + needle.size();
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
      ++i;
    if (i >= s.size() || s[i] != ':')
      return false;
    ++i;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
      ++i;
    value = i;
    return i < s.size();
  }

  static bool get_string(const std::string &s, const char *key, std::string &value) {
    size_t i = 0;
    if (!find_key(s, key, i) || s[i] != '"')
      return false;
    ++i;
    const size_t begin = i;
    while (i < s.size() && s[i] != '"') {
      if (s[i] == '\\' || static_cast<unsigned char>(s[i]) < 32)
        return false;  // No escape sequences in the product fields.
      ++i;
    }
    if (i >= s.size() || i == begin || i - begin > 48)
      return false;
    value = s.substr(begin, i - begin);
    return true;
  }

  static bool get_integer(const std::string &s, const char *key, int &value) {
    size_t i = 0;
    if (!find_key(s, key, i) || !std::isdigit(static_cast<unsigned char>(s[i])))
      return false;
    unsigned n = 0;
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
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
    const std::string s(reinterpret_cast<const char *>(data), len);
    if (s.front() != '{' || s.back() != '}')
      return false;
    // Like the original WXU, only p and v are required. MCU firmware versions differ in the rest; without
    // wk, mwk or sm the fields stay -1, and standby is refused because it needs sm=1 and a matching wk.
    parsed = get_string(s, "p", pid) && get_string(s, "v", version);
    if (parsed) {
      (void) get_integer(s, "wk", wake_gpio);
      (void) get_integer(s, "mwk", mcu_wake_gpio);
      if (get_integer(s, "sm", sleep_mode) && sleep_mode > 2)
        sleep_mode = -1;
      int level = 0;
      if (get_integer(s, "mwkp", level))
        mcu_wake_active_high = level == 1;
    }
    int pulse_ms = 0;
    if (parsed && get_integer(s, "mwkpt", pulse_ms))
      mcu_wake_pulse_ms = pulse_ms < 5 ? 5 : (pulse_ms > 200 ? 200 : static_cast<uint32_t>(pulse_ms));
    return parsed;
  }

  /// True if parsed and the product ID equals expected_pid; an empty expected_pid accepts any product.
  bool matches(const char *expected_pid) const { return parsed && (expected_pid[0] == '\0' || pid == expected_pid); }
};

}  // namespace esphome::tuya
