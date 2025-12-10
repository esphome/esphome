#include "esphome/core/helpers.h"

#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/string_ref.h"

#include <strings.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>

#ifdef USE_ESP32
#include "rom/crc.h"
#endif

namespace esphome {

static const char *const TAG = "helpers";

static const uint16_t CRC16_A001_LE_LUT_L[] = {0x0000, 0xc0c1, 0xc181, 0x0140, 0xc301, 0x03c0, 0x0280, 0xc241,
                                               0xc601, 0x06c0, 0x0780, 0xc741, 0x0500, 0xc5c1, 0xc481, 0x0440};
static const uint16_t CRC16_A001_LE_LUT_H[] = {0x0000, 0xcc01, 0xd801, 0x1400, 0xf001, 0x3c00, 0x2800, 0xe401,
                                               0xa001, 0x6c00, 0x7800, 0xb401, 0x5000, 0x9c01, 0x8801, 0x4400};

#ifndef USE_ESP32
static const uint16_t CRC16_8408_LE_LUT_L[] = {0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
                                               0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7};
static const uint16_t CRC16_8408_LE_LUT_H[] = {0x0000, 0x1081, 0x2102, 0x3183, 0x4204, 0x5285, 0x6306, 0x7387,
                                               0x8408, 0x9489, 0xa50a, 0xb58b, 0xc60c, 0xd68d, 0xe70e, 0xf78f};
#endif

#if !defined(USE_ESP32) || defined(USE_ESP32_VARIANT_ESP32S2)
static const uint16_t CRC16_1021_BE_LUT_L[] = {0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
                                               0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef};
static const uint16_t CRC16_1021_BE_LUT_H[] = {0x0000, 0x1231, 0x2462, 0x3653, 0x48c4, 0x5af5, 0x6ca6, 0x7e97,
                                               0x9188, 0x83b9, 0xb5ea, 0xa7db, 0xd94c, 0xcb7d, 0xfd2e, 0xef1f};
#endif

// Mathematics

uint8_t crc8(const uint8_t *data, uint8_t len, uint8_t crc, uint8_t poly, bool msb_first) {
  while ((len--) != 0u) {
    uint8_t inbyte = *data++;
    if (msb_first) {
      // MSB first processing (for polynomials like 0x31, 0x07)
      crc ^= inbyte;
      for (uint8_t i = 8; i != 0u; i--) {
        if (crc & 0x80) {
          crc = (crc << 1) ^ poly;
        } else {
          crc <<= 1;
        }
      }
    } else {
      // LSB first processing (default for Dallas/Maxim 0x8C)
      for (uint8_t i = 8; i != 0u; i--) {
        bool mix = (crc ^ inbyte) & 0x01;
        crc >>= 1;
        if (mix)
          crc ^= poly;
        inbyte >>= 1;
      }
    }
  }
  return crc;
}

uint16_t crc16(const uint8_t *data, uint16_t len, uint16_t crc, uint16_t reverse_poly, bool refin, bool refout) {
#ifdef USE_ESP32
  if (reverse_poly == 0x8408) {
    crc = crc16_le(refin ? crc : (crc ^ 0xffff), data, len);
    return refout ? crc : (crc ^ 0xffff);
  }
#endif
  if (refin) {
    crc ^= 0xffff;
  }
#ifndef USE_ESP32
  if (reverse_poly == 0x8408) {
    while (len--) {
      uint8_t combo = crc ^ (uint8_t) *data++;
      crc = (crc >> 8) ^ CRC16_8408_LE_LUT_L[combo & 0x0F] ^ CRC16_8408_LE_LUT_H[combo >> 4];
    }
  } else
#endif
  {
    if (reverse_poly == 0xa001) {
      while (len--) {
        uint8_t combo = crc ^ (uint8_t) *data++;
        crc = (crc >> 8) ^ CRC16_A001_LE_LUT_L[combo & 0x0F] ^ CRC16_A001_LE_LUT_H[combo >> 4];
      }
    } else {
      while (len--) {
        crc ^= *data++;
        for (uint8_t i = 0; i < 8; i++) {
          if (crc & 0x0001) {
            crc = (crc >> 1) ^ reverse_poly;
          } else {
            crc >>= 1;
          }
        }
      }
    }
  }
  return refout ? (crc ^ 0xffff) : crc;
}

uint16_t crc16be(const uint8_t *data, uint16_t len, uint16_t crc, uint16_t poly, bool refin, bool refout) {
#if defined(USE_ESP32) && !defined(USE_ESP32_VARIANT_ESP32S2)
  if (poly == 0x1021) {
    crc = crc16_be(refin ? crc : (crc ^ 0xffff), data, len);
    return refout ? crc : (crc ^ 0xffff);
  }
#endif
  if (refin) {
    crc ^= 0xffff;
  }
#if !defined(USE_ESP32) || defined(USE_ESP32_VARIANT_ESP32S2)
  if (poly == 0x1021) {
    while (len--) {
      uint8_t combo = (crc >> 8) ^ *data++;
      crc = (crc << 8) ^ CRC16_1021_BE_LUT_L[combo & 0x0F] ^ CRC16_1021_BE_LUT_H[combo >> 4];
    }
  } else {
#endif
    while (len--) {
      crc ^= (((uint16_t) *data++) << 8);
      for (uint8_t i = 0; i < 8; i++) {
        if (crc & 0x8000) {
          crc = (crc << 1) ^ poly;
        } else {
          crc <<= 1;
        }
      }
    }
#if !defined(USE_ESP32) || defined(USE_ESP32_VARIANT_ESP32S2)
  }
#endif
  return refout ? (crc ^ 0xffff) : crc;
}

uint32_t fnv1_hash(const char *str) {
  uint32_t hash = 2166136261UL;
  if (str) {
    while (*str) {
      hash *= 16777619UL;
      hash ^= *str++;
    }
  }
  return hash;
}

float random_float() { return static_cast<float>(random_uint32()) / static_cast<float>(UINT32_MAX); }

// Strings

bool str_equals_case_insensitive(const std::string &a, const std::string &b) {
  return strcasecmp(a.c_str(), b.c_str()) == 0;
}
#if __cplusplus >= 202002L
bool str_startswith(const std::string &str, const std::string &start) { return str.starts_with(start); }
bool str_endswith(const std::string &str, const std::string &end) { return str.ends_with(end); }
#else
bool str_startswith(const std::string &str, const std::string &start) { return str.rfind(start, 0) == 0; }
bool str_endswith(const std::string &str, const std::string &end) {
  return str.rfind(end) == (str.size() - end.size());
}
#endif
std::string str_truncate(const std::string &str, size_t length) {
  return str.length() > length ? str.substr(0, length) : str;
}
std::string str_until(const char *str, char ch) {
  const char *pos = strchr(str, ch);
  return pos == nullptr ? std::string(str) : std::string(str, pos - str);
}
std::string str_until(const std::string &str, char ch) { return str.substr(0, str.find(ch)); }
// wrapper around std::transform to run safely on functions from the ctype.h header
// see https://en.cppreference.com/w/cpp/string/byte/toupper#Notes
template<int (*fn)(int)> std::string str_ctype_transform(const std::string &str) {
  std::string result;
  result.resize(str.length());
  std::transform(str.begin(), str.end(), result.begin(), [](unsigned char ch) { return fn(ch); });
  return result;
}
std::string str_lower_case(const std::string &str) { return str_ctype_transform<std::tolower>(str); }
std::string str_upper_case(const std::string &str) { return str_ctype_transform<std::toupper>(str); }
std::string str_snake_case(const std::string &str) {
  std::string result;
  result.resize(str.length());
  std::transform(str.begin(), str.end(), result.begin(), ::tolower);
  std::replace(result.begin(), result.end(), ' ', '_');
  return result;
}
std::string str_sanitize(const std::string &str) {
  std::string out = str;
  std::replace_if(
      out.begin(), out.end(),
      [](const char &c) {
        return c != '-' && c != '_' && (c < '0' || c > '9') && (c < 'a' || c > 'z') && (c < 'A' || c > 'Z');
      },
      '_');
  return out;
}
std::string str_snprintf(const char *fmt, size_t len, ...) {
  std::string str;
  va_list args;

  str.resize(len);
  va_start(args, len);
  size_t out_length = vsnprintf(&str[0], len + 1, fmt, args);
  va_end(args);

  if (out_length < len)
    str.resize(out_length);

  return str;
}
std::string str_sprintf(const char *fmt, ...) {
  std::string str;
  va_list args;

  va_start(args, fmt);
  size_t length = vsnprintf(nullptr, 0, fmt, args);
  va_end(args);

  str.resize(length);
  va_start(args, fmt);
  vsnprintf(&str[0], length + 1, fmt, args);
  va_end(args);

  return str;
}

// Maximum size for name with suffix: 120 (max friendly name) + 1 (separator) + 6 (MAC suffix) + 1 (null term)
static constexpr size_t MAX_NAME_WITH_SUFFIX_SIZE = 128;

std::string make_name_with_suffix(const char *name, size_t name_len, char sep, const char *suffix_ptr,
                                  size_t suffix_len) {
  char buffer[MAX_NAME_WITH_SUFFIX_SIZE];
  size_t total_len = name_len + 1 + suffix_len;

  // Silently truncate if needed: prioritize keeping the full suffix
  if (total_len >= MAX_NAME_WITH_SUFFIX_SIZE) {
    // NOTE: This calculation could underflow if suffix_len >= MAX_NAME_WITH_SUFFIX_SIZE - 2,
    // but this is safe because this helper is only called with small suffixes:
    // MAC suffixes (6-12 bytes), ".local" (5 bytes), etc.
    name_len = MAX_NAME_WITH_SUFFIX_SIZE - suffix_len - 2;  // -2 for separator and null terminator
    total_len = name_len + 1 + suffix_len;
  }

  memcpy(buffer, name, name_len);
  buffer[name_len] = sep;
  memcpy(buffer + name_len + 1, suffix_ptr, suffix_len);
  buffer[total_len] = '\0';
  return std::string(buffer, total_len);
}

std::string make_name_with_suffix(const std::string &name, char sep, const char *suffix_ptr, size_t suffix_len) {
  return make_name_with_suffix(name.c_str(), name.size(), sep, suffix_ptr, suffix_len);
}

// Parsing & formatting

size_t parse_hex(const char *str, size_t length, uint8_t *data, size_t count) {
  uint8_t val;
  size_t chars = std::min(length, 2 * count);
  for (size_t i = 2 * count - chars; i < 2 * count; i++, str++) {
    if (*str >= '0' && *str <= '9') {
      val = *str - '0';
    } else if (*str >= 'A' && *str <= 'F') {
      val = 10 + (*str - 'A');
    } else if (*str >= 'a' && *str <= 'f') {
      val = 10 + (*str - 'a');
    } else {
      return 0;
    }
    data[i >> 1] = !(i & 1) ? val << 4 : data[i >> 1] | val;
  }
  return chars;
}

std::string format_mac_address_pretty(const uint8_t *mac) {
  char buf[18];
  format_mac_addr_upper(mac, buf);
  return std::string(buf);
}

std::string format_hex(const uint8_t *data, size_t length) {
  std::string ret;
  ret.resize(length * 2);
  for (size_t i = 0; i < length; i++) {
    ret[2 * i] = format_hex_char(data[i] >> 4);
    ret[2 * i + 1] = format_hex_char(data[i] & 0x0F);
  }
  return ret;
}
std::string format_hex(const std::vector<uint8_t> &data) { return format_hex(data.data(), data.size()); }

// Shared implementation for uint8_t and string hex formatting
static std::string format_hex_pretty_uint8(const uint8_t *data, size_t length, char separator, bool show_length) {
  if (data == nullptr || length == 0)
    return "";
  std::string ret;
  uint8_t multiple = separator ? 3 : 2;  // 3 if separator is not \0, 2 otherwise
  ret.resize(multiple * length - (separator ? 1 : 0));
  for (size_t i = 0; i < length; i++) {
    ret[multiple * i] = format_hex_pretty_char(data[i] >> 4);
    ret[multiple * i + 1] = format_hex_pretty_char(data[i] & 0x0F);
    if (separator && i != length - 1)
      ret[multiple * i + 2] = separator;
  }
  if (show_length && length > 4)
    return ret + " (" + std::to_string(length) + ")";
  return ret;
}

std::string format_hex_pretty(const uint8_t *data, size_t length, char separator, bool show_length) {
  return format_hex_pretty_uint8(data, length, separator, show_length);
}
std::string format_hex_pretty(const std::vector<uint8_t> &data, char separator, bool show_length) {
  return format_hex_pretty(data.data(), data.size(), separator, show_length);
}

std::string format_hex_pretty(const uint16_t *data, size_t length, char separator, bool show_length) {
  if (data == nullptr || length == 0)
    return "";
  std::string ret;
  uint8_t multiple = separator ? 5 : 4;  // 5 if separator is not \0, 4 otherwise
  ret.resize(multiple * length - (separator ? 1 : 0));
  for (size_t i = 0; i < length; i++) {
    ret[multiple * i] = format_hex_pretty_char((data[i] & 0xF000) >> 12);
    ret[multiple * i + 1] = format_hex_pretty_char((data[i] & 0x0F00) >> 8);
    ret[multiple * i + 2] = format_hex_pretty_char((data[i] & 0x00F0) >> 4);
    ret[multiple * i + 3] = format_hex_pretty_char(data[i] & 0x000F);
    if (separator && i != length - 1)
      ret[multiple * i + 4] = separator;
  }
  if (show_length && length > 4)
    return ret + " (" + std::to_string(length) + ")";
  return ret;
}
std::string format_hex_pretty(const std::vector<uint16_t> &data, char separator, bool show_length) {
  return format_hex_pretty(data.data(), data.size(), separator, show_length);
}
std::string format_hex_pretty(const std::string &data, char separator, bool show_length) {
  return format_hex_pretty_uint8(reinterpret_cast<const uint8_t *>(data.data()), data.length(), separator, show_length);
}

std::string format_bin(const uint8_t *data, size_t length) {
  std::string result;
  result.resize(length * 8);
  for (size_t byte_idx = 0; byte_idx < length; byte_idx++) {
    for (size_t bit_idx = 0; bit_idx < 8; bit_idx++) {
      result[byte_idx * 8 + bit_idx] = ((data[byte_idx] >> (7 - bit_idx)) & 1) + '0';
    }
  }

  return result;
}

ParseOnOffState parse_on_off(const char *str, const char *on, const char *off) {
  if (on == nullptr && strcasecmp(str, "on") == 0)
    return PARSE_ON;
  if (on != nullptr && strcasecmp(str, on) == 0)
    return PARSE_ON;
  if (off == nullptr && strcasecmp(str, "off") == 0)
    return PARSE_OFF;
  if (off != nullptr && strcasecmp(str, off) == 0)
    return PARSE_OFF;
  if (strcasecmp(str, "toggle") == 0)
    return PARSE_TOGGLE;

  return PARSE_NONE;
}

static inline void normalize_accuracy_decimals(float &value, int8_t &accuracy_decimals) {
  if (accuracy_decimals < 0) {
    auto multiplier = powf(10.0f, accuracy_decimals);
    value = roundf(value * multiplier) / multiplier;
    accuracy_decimals = 0;
  }
}

std::string value_accuracy_to_string(float value, int8_t accuracy_decimals) {
  normalize_accuracy_decimals(value, accuracy_decimals);
  char tmp[32];  // should be enough, but we should maybe improve this at some point.
  snprintf(tmp, sizeof(tmp), "%.*f", accuracy_decimals, value);
  return std::string(tmp);
}

std::string value_accuracy_with_uom_to_string(float value, int8_t accuracy_decimals, StringRef unit_of_measurement) {
  normalize_accuracy_decimals(value, accuracy_decimals);
  // Buffer sized for float (up to ~15 chars) + space + typical UOM (usually <20 chars like "μS/cm")
  // snprintf truncates safely if exceeded, though ESPHome UOMs are typically short
  char tmp[64];
  if (unit_of_measurement.empty()) {
    snprintf(tmp, sizeof(tmp), "%.*f", accuracy_decimals, value);
  } else {
    snprintf(tmp, sizeof(tmp), "%.*f %s", accuracy_decimals, value, unit_of_measurement.c_str());
  }
  return std::string(tmp);
}

int8_t step_to_accuracy_decimals(float step) {
  // use printf %g to find number of digits based on temperature step
  char buf[32];
  snprintf(buf, sizeof buf, "%.5g", step);

  std::string str{buf};
  size_t dot_pos = str.find('.');
  if (dot_pos == std::string::npos)
    return 0;

  return str.length() - dot_pos - 1;
}

// Use C-style string constant to store in ROM instead of RAM (saves 24 bytes)
static constexpr const char *BASE64_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                            "abcdefghijklmnopqrstuvwxyz"
                                            "0123456789+/";

// Helper function to find the index of a base64 character in the lookup table.
// Returns the character's position (0-63) if found, or 0 if not found.
// NOTE: This returns 0 for both 'A' (valid base64 char at index 0) and invalid characters.
// This is safe because is_base64() is ALWAYS checked before calling this function,
// preventing invalid characters from ever reaching here. The base64_decode function
// stops processing at the first invalid character due to the is_base64() check in its
// while loop condition, making this edge case harmless in practice.
static inline uint8_t base64_find_char(char c) {
  const char *pos = strchr(BASE64_CHARS, c);
  return pos ? (pos - BASE64_CHARS) : 0;
}

static inline bool is_base64(char c) { return (isalnum(c) || (c == '+') || (c == '/')); }

std::string base64_encode(const std::vector<uint8_t> &buf) { return base64_encode(buf.data(), buf.size()); }

std::string base64_encode(const uint8_t *buf, size_t buf_len) {
  std::string ret;
  int i = 0;
  int j = 0;
  char char_array_3[3];
  char char_array_4[4];

  while (buf_len--) {
    char_array_3[i++] = *(buf++);
    if (i == 3) {
      char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
      char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
      char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
      char_array_4[3] = char_array_3[2] & 0x3f;

      for (i = 0; (i < 4); i++)
        ret += BASE64_CHARS[static_cast<uint8_t>(char_array_4[i])];
      i = 0;
    }
  }

  if (i) {
    for (j = i; j < 3; j++)
      char_array_3[j] = '\0';

    char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
    char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
    char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
    char_array_4[3] = char_array_3[2] & 0x3f;

    for (j = 0; (j < i + 1); j++)
      ret += BASE64_CHARS[static_cast<uint8_t>(char_array_4[j])];

    while ((i++ < 3))
      ret += '=';
  }

  return ret;
}

size_t base64_decode(const std::string &encoded_string, uint8_t *buf, size_t buf_len) {
  int in_len = encoded_string.size();
  int i = 0;
  int j = 0;
  int in = 0;
  size_t out = 0;
  uint8_t char_array_4[4], char_array_3[3];
  bool truncated = false;

  // SAFETY: The loop condition checks is_base64() before processing each character.
  // This ensures base64_find_char() is only called on valid base64 characters,
  // preventing the edge case where invalid chars would return 0 (same as 'A').
  while (in_len-- && (encoded_string[in] != '=') && is_base64(encoded_string[in])) {
    char_array_4[i++] = encoded_string[in];
    in++;
    if (i == 4) {
      for (i = 0; i < 4; i++)
        char_array_4[i] = base64_find_char(char_array_4[i]);

      char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
      char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
      char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

      for (i = 0; i < 3; i++) {
        if (out < buf_len) {
          buf[out++] = char_array_3[i];
        } else {
          truncated = true;
        }
      }
      i = 0;
    }
  }

  if (i) {
    for (j = i; j < 4; j++)
      char_array_4[j] = 0;

    for (j = 0; j < 4; j++)
      char_array_4[j] = base64_find_char(char_array_4[j]);

    char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
    char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
    char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

    for (j = 0; j < i - 1; j++) {
      if (out < buf_len) {
        buf[out++] = char_array_3[j];
      } else {
        truncated = true;
      }
    }
  }

  if (truncated) {
    ESP_LOGW(TAG, "Base64 decode: buffer too small, truncating");
  }

  return out;
}

std::vector<uint8_t> base64_decode(const std::string &encoded_string) {
  // Calculate maximum decoded size: every 4 base64 chars = 3 bytes
  size_t max_len = ((encoded_string.size() + 3) / 4) * 3;
  std::vector<uint8_t> ret(max_len);
  size_t actual_len = base64_decode(encoded_string, ret.data(), max_len);
  ret.resize(actual_len);
  return ret;
}

// Colors

float gamma_correct(float value, float gamma) {
  if (value <= 0.0f)
    return 0.0f;
  if (gamma <= 0.0f)
    return value;

  return powf(value, gamma);
}
float gamma_uncorrect(float value, float gamma) {
  if (value <= 0.0f)
    return 0.0f;
  if (gamma <= 0.0f)
    return value;

  return powf(value, 1 / gamma);
}

void rgb_to_hsv(float red, float green, float blue, int &hue, float &saturation, float &value) {
  float max_color_value = std::max(std::max(red, green), blue);
  float min_color_value = std::min(std::min(red, green), blue);
  float delta = max_color_value - min_color_value;

  if (delta == 0) {
    hue = 0;
  } else if (max_color_value == red) {
    hue = int(fmod(((60 * ((green - blue) / delta)) + 360), 360));
  } else if (max_color_value == green) {
    hue = int(fmod(((60 * ((blue - red) / delta)) + 120), 360));
  } else if (max_color_value == blue) {
    hue = int(fmod(((60 * ((red - green) / delta)) + 240), 360));
  }

  if (max_color_value == 0) {
    saturation = 0;
  } else {
    saturation = delta / max_color_value;
  }

  value = max_color_value;
}
void hsv_to_rgb(int hue, float saturation, float value, float &red, float &green, float &blue) {
  float chroma = value * saturation;
  float hue_prime = fmod(hue / 60.0, 6);
  float intermediate = chroma * (1 - fabs(fmod(hue_prime, 2) - 1));
  float delta = value - chroma;

  if (0 <= hue_prime && hue_prime < 1) {
    red = chroma;
    green = intermediate;
    blue = 0;
  } else if (1 <= hue_prime && hue_prime < 2) {
    red = intermediate;
    green = chroma;
    blue = 0;
  } else if (2 <= hue_prime && hue_prime < 3) {
    red = 0;
    green = chroma;
    blue = intermediate;
  } else if (3 <= hue_prime && hue_prime < 4) {
    red = 0;
    green = intermediate;
    blue = chroma;
  } else if (4 <= hue_prime && hue_prime < 5) {
    red = intermediate;
    green = 0;
    blue = chroma;
  } else if (5 <= hue_prime && hue_prime < 6) {
    red = chroma;
    green = 0;
    blue = intermediate;
  } else {
    red = 0;
    green = 0;
    blue = 0;
  }

  red += delta;
  green += delta;
  blue += delta;
}

uint8_t HighFrequencyLoopRequester::num_requests = 0;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
void HighFrequencyLoopRequester::start() {
  if (this->started_)
    return;
  num_requests++;
  this->started_ = true;
}
void HighFrequencyLoopRequester::stop() {
  if (!this->started_)
    return;
  num_requests--;
  this->started_ = false;
}
bool HighFrequencyLoopRequester::is_high_frequency() { return num_requests > 0; }

std::string get_mac_address() {
  uint8_t mac[6];
  get_mac_address_raw(mac);
  char buf[13];
  format_mac_addr_lower_no_sep(mac, buf);
  return std::string(buf);
}

std::string get_mac_address_pretty() {
  char buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  return std::string(get_mac_address_pretty_into_buffer(buf));
}

void get_mac_address_into_buffer(std::span<char, MAC_ADDRESS_BUFFER_SIZE> buf) {
  uint8_t mac[6];
  get_mac_address_raw(mac);
  format_mac_addr_lower_no_sep(mac, buf.data());
}

const char *get_mac_address_pretty_into_buffer(std::span<char, MAC_ADDRESS_PRETTY_BUFFER_SIZE> buf) {
  uint8_t mac[6];
  get_mac_address_raw(mac);
  format_mac_addr_upper(mac, buf.data());
  return buf.data();
}

#ifndef USE_ESP32
bool has_custom_mac_address() { return false; }
#endif

bool mac_address_is_valid(const uint8_t *mac) {
  bool is_all_zeros = true;
  bool is_all_ones = true;

  for (uint8_t i = 0; i < 6; i++) {
    if (mac[i] != 0) {
      is_all_zeros = false;
    }
    if (mac[i] != 0xFF) {
      is_all_ones = false;
    }
  }
  return !(is_all_zeros || is_all_ones);
}

void IRAM_ATTR HOT delay_microseconds_safe(uint32_t us) {
  // avoids CPU locks that could trigger WDT or affect WiFi/BT stability
  uint32_t start = micros();

  const uint32_t lag = 5000;  // microseconds, specifies the maximum time for a CPU busy-loop.
                              // it must be larger than the worst-case duration of a delay(1) call (hardware tasks)
                              // 5ms is conservative, it could be reduced when exact BT/WiFi stack delays are known
  if (us > lag) {
    delay((us - lag) / 1000UL);  // note: in disabled-interrupt contexts delay() won't actually sleep
    while (micros() - start < us - lag)
      delay(1);  // in those cases, this loop allows to yield for BT/WiFi stack tasks
  }
  while (micros() - start < us)  // fine delay the remaining usecs
    ;
}

}  // namespace esphome
