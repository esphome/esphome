#include "esphome/core/alloc_helpers.h"

#include "esphome/core/helpers.h"

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>

namespace esphome {

// --- String helpers ---

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

std::string str_upper_case(const std::string &str) {
  std::string result;
  result.resize(str.length());
  std::transform(str.begin(), str.end(), result.begin(), [](unsigned char ch) { return std::toupper(ch); });
  return result;
}

std::string str_snake_case(const std::string &str) {
  std::string result = str;
  for (char &c : result) {
    c = to_snake_case_char(c);
  }
  return result;
}

std::string str_sanitize(const std::string &str) {
  std::string result;
  result.resize(str.size());
  str_sanitize_to(&result[0], str.size() + 1, str.c_str());
  return result;
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

// --- Value formatting helpers ---

std::string value_accuracy_to_string(float value, int8_t accuracy_decimals) {
  char buf[VALUE_ACCURACY_MAX_LEN];
  value_accuracy_to_buf(buf, value, accuracy_decimals);
  return std::string(buf);
}

// --- Base64 helpers ---

static constexpr const char *BASE64_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                            "abcdefghijklmnopqrstuvwxyz"
                                            "0123456789+/";

// Encode 3 input bytes to 4 base64 characters, append 'count' to ret.
static inline void base64_encode_triple(const char *char_array_3, int count, std::string &ret) {
  char char_array_4[4];
  char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
  char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
  char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
  char_array_4[3] = char_array_3[2] & 0x3f;

  for (int j = 0; j < count; j++)
    ret += BASE64_CHARS[static_cast<uint8_t>(char_array_4[j])];
}

std::string base64_encode(const std::vector<uint8_t> &buf) { return base64_encode(buf.data(), buf.size()); }

std::string base64_encode(const uint8_t *buf, size_t buf_len) {
  std::string ret;
  int i = 0;
  char char_array_3[3];

  while (buf_len--) {
    char_array_3[i++] = *(buf++);
    if (i == 3) {
      base64_encode_triple(char_array_3, 4, ret);
      i = 0;
    }
  }

  if (i) {
    for (int j = i; j < 3; j++)
      char_array_3[j] = '\0';

    base64_encode_triple(char_array_3, i + 1, ret);

    while ((i++ < 3))
      ret += '=';
  }

  return ret;
}

std::vector<uint8_t> base64_decode(const std::string &encoded_string) {
  // Calculate maximum decoded size: every 4 base64 chars = 3 bytes
  size_t max_len = ((encoded_string.size() + 3) / 4) * 3;
  std::vector<uint8_t> ret(max_len);
  size_t actual_len = base64_decode(encoded_string, ret.data(), max_len);
  ret.resize(actual_len);
  return ret;
}

// --- Hex/binary formatting helpers ---

std::string format_mac_address_pretty(const uint8_t *mac) {
  char buf[18];
  format_mac_addr_upper(mac, buf);
  return std::string(buf);
}

std::string format_hex(const uint8_t *data, size_t length) {
  std::string ret;
  ret.resize(length * 2);
  format_hex_to(&ret[0], length * 2 + 1, data, length);
  return ret;
}

std::string format_hex(const std::vector<uint8_t> &data) { return format_hex(data.data(), data.size()); }

// Shared implementation for uint8_t and string hex pretty formatting
static std::string format_hex_pretty_uint8(const uint8_t *data, size_t length, char separator, bool show_length) {
  if (data == nullptr || length == 0)
    return "";
  std::string ret;
  size_t hex_len = separator ? (length * 3 - 1) : (length * 2);
  ret.resize(hex_len);
  format_hex_pretty_to(&ret[0], hex_len + 1, data, length, separator);
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
  size_t hex_len = separator ? (length * 5 - 1) : (length * 4);
  ret.resize(hex_len);
  format_hex_pretty_to(&ret[0], hex_len + 1, data, length, separator);
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
  format_bin_to(&result[0], length * 8 + 1, data, length);
  return result;
}

// --- MAC address helpers ---

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

}  // namespace esphome
