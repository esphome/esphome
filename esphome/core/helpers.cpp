#include "esphome/core/helpers.h"

#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>

#ifdef USE_HOST
#ifndef _WIN32
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#endif
#include <unistd.h>
#endif
#if defined(USE_ESP8266)
#include <osapi.h>
#include <user_interface.h>
// for xt_rsil()/xt_wsr_ps()
#include <Arduino.h>
#elif defined(USE_ESP32_FRAMEWORK_ARDUINO)
#include <Esp.h>
#elif defined(USE_ESP_IDF)
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_system.h"
#elif defined(USE_RP2040)
#if defined(USE_WIFI)
#include <WiFi.h>
#endif
#include <hardware/structs/rosc.h>
#include <hardware/sync.h>
#elif defined(USE_HOST)
#include <limits>
#include <random>
#endif
#ifdef USE_ESP32
#include "esp32/rom/crc.h"

#include "esp_efuse.h"
#include "esp_efuse_table.h"
#endif

#ifdef USE_LIBRETINY
#include <WiFi.h>  // for macAddress()
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

static const uint16_t CRC16_1021_BE_LUT_L[] = {0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
                                               0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef};
static const uint16_t CRC16_1021_BE_LUT_H[] = {0x0000, 0x1231, 0x2462, 0x3653, 0x48c4, 0x5af5, 0x6ca6, 0x7e97,
                                               0x9188, 0x83b9, 0xb5ea, 0xa7db, 0xd94c, 0xcb7d, 0xfd2e, 0xef1f};
#endif

// STL backports

#if _GLIBCXX_RELEASE < 8
std::string to_string(int value) { return str_snprintf("%d", 32, value); }                   // NOLINT
std::string to_string(long value) { return str_snprintf("%ld", 32, value); }                 // NOLINT
std::string to_string(long long value) { return str_snprintf("%lld", 32, value); }           // NOLINT
std::string to_string(unsigned value) { return str_snprintf("%u", 32, value); }              // NOLINT
std::string to_string(unsigned long value) { return str_snprintf("%lu", 32, value); }        // NOLINT
std::string to_string(unsigned long long value) { return str_snprintf("%llu", 32, value); }  // NOLINT
std::string to_string(float value) { return str_snprintf("%f", 32, value); }
std::string to_string(double value) { return str_snprintf("%f", 32, value); }
std::string to_string(long double value) { return str_snprintf("%Lf", 32, value); }
#endif

// Mathematics

float lerp(float completion, float start, float end) { return start + (end - start) * completion; }
uint8_t crc8(const uint8_t *data, uint8_t len) {
  uint8_t crc = 0;

  while ((len--) != 0u) {
    uint8_t inbyte = *data++;
    for (uint8_t i = 8; i != 0u; i--) {
      bool mix = (crc ^ inbyte) & 0x01;
      crc >>= 1;
      if (mix)
        crc ^= 0x8C;
      inbyte >>= 1;
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
  return refout ? (crc ^ 0xffff) : crc;
}

uint16_t crc16be(const uint8_t *data, uint16_t len, uint16_t crc, uint16_t poly, bool refin, bool refout) {
#ifdef USE_ESP32
  if (poly == 0x1021) {
    crc = crc16_be(refin ? crc : (crc ^ 0xffff), data, len);
    return refout ? crc : (crc ^ 0xffff);
  }
#endif
  if (refin) {
    crc ^= 0xffff;
  }
#ifndef USE_ESP32
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
#ifndef USE_ESP32
  }
#endif
  return refout ? (crc ^ 0xffff) : crc;
}

uint32_t fnv1_hash(const std::string &str) {
  uint32_t hash = 2166136261UL;
  for (char c : str) {
    hash *= 16777619UL;
    hash ^= c;
  }
  return hash;
}

uint32_t random_uint32() {
#ifdef USE_ESP32
  return esp_random();
#elif defined(USE_ESP8266)
  return os_random();
#elif defined(USE_RP2040)
  uint32_t result = 0;
  for (uint8_t i = 0; i < 32; i++) {
    result <<= 1;
    result |= rosc_hw->randombit;
  }
  return result;
#elif defined(USE_LIBRETINY)
  return rand();
#elif defined(USE_HOST)
  std::random_device dev;
  std::mt19937 rng(dev());
  std::uniform_int_distribution<uint32_t> dist(0, std::numeric_limits<uint32_t>::max());
  return dist(rng);
#else
#error "No random source available for this configuration."
#endif
}
float random_float() { return static_cast<float>(random_uint32()) / static_cast<float>(UINT32_MAX); }
bool random_bytes(uint8_t *data, size_t len) {
#ifdef USE_ESP32
  esp_fill_random(data, len);
  return true;
#elif defined(USE_ESP8266)
  return os_get_random(data, len) == 0;
#elif defined(USE_RP2040)
  while (len-- != 0) {
    uint8_t result = 0;
    for (uint8_t i = 0; i < 8; i++) {
      result <<= 1;
      result |= rosc_hw->randombit;
    }
    *data++ = result;
  }
  return true;
#elif defined(USE_LIBRETINY)
  lt_rand_bytes(data, len);
  return true;
#elif defined(USE_HOST)
  FILE *fp = fopen("/dev/urandom", "r");
  if (fp == nullptr) {
    ESP_LOGW(TAG, "Could not open /dev/urandom, errno=%d", errno);
    exit(1);
  }
  size_t read = fread(data, 1, len, fp);
  if (read != len) {
    ESP_LOGW(TAG, "Not enough data from /dev/urandom");
    exit(1);
  }
  fclose(fp);
  return true;
#else
#error "No random source available for this configuration."
#endif
}

// Strings

bool str_equals_case_insensitive(const std::string &a, const std::string &b) {
  return strcasecmp(a.c_str(), b.c_str()) == 0;
}
bool str_startswith(const std::string &str, const std::string &start) { return str.rfind(start, 0) == 0; }
bool str_endswith(const std::string &str, const std::string &end) {
  return str.rfind(end) == (str.size() - end.size());
}
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
        return !(c == '-' || c == '_' || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
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

static char format_hex_char(uint8_t v) { return v >= 10 ? 'a' + (v - 10) : '0' + v; }
std::string format_hex(const uint8_t *data, size_t length) {
  std::string ret;
  ret.resize(length * 2);
  for (size_t i = 0; i < length; i++) {
    ret[2 * i] = format_hex_char((data[i] & 0xF0) >> 4);
    ret[2 * i + 1] = format_hex_char(data[i] & 0x0F);
  }
  return ret;
}
std::string format_hex(const std::vector<uint8_t> &data) { return format_hex(data.data(), data.size()); }

static char format_hex_pretty_char(uint8_t v) { return v >= 10 ? 'A' + (v - 10) : '0' + v; }
std::string format_hex_pretty(const uint8_t *data, size_t length) {
  if (length == 0)
    return "";
  std::string ret;
  ret.resize(3 * length - 1);
  for (size_t i = 0; i < length; i++) {
    ret[3 * i] = format_hex_pretty_char((data[i] & 0xF0) >> 4);
    ret[3 * i + 1] = format_hex_pretty_char(data[i] & 0x0F);
    if (i != length - 1)
      ret[3 * i + 2] = '.';
  }
  if (length > 4)
    return ret + " (" + to_string(length) + ")";
  return ret;
}
std::string format_hex_pretty(const std::vector<uint8_t> &data) { return format_hex_pretty(data.data(), data.size()); }

std::string format_hex_pretty(const uint16_t *data, size_t length) {
  if (length == 0)
    return "";
  std::string ret;
  ret.resize(5 * length - 1);
  for (size_t i = 0; i < length; i++) {
    ret[5 * i] = format_hex_pretty_char((data[i] & 0xF000) >> 12);
    ret[5 * i + 1] = format_hex_pretty_char((data[i] & 0x0F00) >> 8);
    ret[5 * i + 2] = format_hex_pretty_char((data[i] & 0x00F0) >> 4);
    ret[5 * i + 3] = format_hex_pretty_char(data[i] & 0x000F);
    if (i != length - 1)
      ret[5 * i + 2] = '.';
  }
  if (length > 4)
    return ret + " (" + to_string(length) + ")";
  return ret;
}
std::string format_hex_pretty(const std::vector<uint16_t> &data) { return format_hex_pretty(data.data(), data.size()); }

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

std::string value_accuracy_to_string(float value, int8_t accuracy_decimals) {
  if (accuracy_decimals < 0) {
    auto multiplier = powf(10.0f, accuracy_decimals);
    value = roundf(value * multiplier) / multiplier;
    accuracy_decimals = 0;
  }
  char tmp[32];  // should be enough, but we should maybe improve this at some point.
  snprintf(tmp, sizeof(tmp), "%.*f", accuracy_decimals, value);
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

static const std::string BASE64_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                        "abcdefghijklmnopqrstuvwxyz"
                                        "0123456789+/";

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
        ret += BASE64_CHARS[char_array_4[i]];
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
      ret += BASE64_CHARS[char_array_4[j]];

    while ((i++ < 3))
      ret += '=';
  }

  return ret;
}

size_t base64_decode(const std::string &encoded_string, uint8_t *buf, size_t buf_len) {
  std::vector<uint8_t> decoded = base64_decode(encoded_string);
  if (decoded.size() > buf_len) {
    ESP_LOGW(TAG, "Base64 decode: buffer too small, truncating");
    decoded.resize(buf_len);
  }
  memcpy(buf, decoded.data(), decoded.size());
  return decoded.size();
}

std::vector<uint8_t> base64_decode(const std::string &encoded_string) {
  int in_len = encoded_string.size();
  int i = 0;
  int j = 0;
  int in = 0;
  uint8_t char_array_4[4], char_array_3[3];
  std::vector<uint8_t> ret;

  while (in_len-- && (encoded_string[in] != '=') && is_base64(encoded_string[in])) {
    char_array_4[i++] = encoded_string[in];
    in++;
    if (i == 4) {
      for (i = 0; i < 4; i++)
        char_array_4[i] = BASE64_CHARS.find(char_array_4[i]);

      char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
      char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
      char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

      for (i = 0; (i < 3); i++)
        ret.push_back(char_array_3[i]);
      i = 0;
    }
  }

  if (i) {
    for (j = i; j < 4; j++)
      char_array_4[j] = 0;

    for (j = 0; j < 4; j++)
      char_array_4[j] = BASE64_CHARS.find(char_array_4[j]);

    char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
    char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
    char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

    for (j = 0; (j < i - 1); j++)
      ret.push_back(char_array_3[j]);
  }

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

// System APIs
#if defined(USE_ESP8266) || defined(USE_RP2040) || defined(USE_HOST)
// ESP8266 doesn't have mutexes, but that shouldn't be an issue as it's single-core and non-preemptive OS.
Mutex::Mutex() {}
void Mutex::lock() {}
bool Mutex::try_lock() { return true; }
void Mutex::unlock() {}
#elif defined(USE_ESP32) || defined(USE_LIBRETINY)
Mutex::Mutex() { handle_ = xSemaphoreCreateMutex(); }
void Mutex::lock() { xSemaphoreTake(this->handle_, portMAX_DELAY); }
bool Mutex::try_lock() { return xSemaphoreTake(this->handle_, 0) == pdTRUE; }
void Mutex::unlock() { xSemaphoreGive(this->handle_); }
#endif

#if defined(USE_ESP8266)
IRAM_ATTR InterruptLock::InterruptLock() { state_ = xt_rsil(15); }
IRAM_ATTR InterruptLock::~InterruptLock() { xt_wsr_ps(state_); }
#elif defined(USE_ESP32) || defined(USE_LIBRETINY)
// only affects the executing core
// so should not be used as a mutex lock, only to get accurate timing
IRAM_ATTR InterruptLock::InterruptLock() { portDISABLE_INTERRUPTS(); }
IRAM_ATTR InterruptLock::~InterruptLock() { portENABLE_INTERRUPTS(); }
#elif defined(USE_RP2040)
IRAM_ATTR InterruptLock::InterruptLock() { state_ = save_and_disable_interrupts(); }
IRAM_ATTR InterruptLock::~InterruptLock() { restore_interrupts(state_); }
#endif

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

void get_mac_address_raw(uint8_t *mac) {  // NOLINT(readability-non-const-parameter)
#if defined(USE_HOST)
  static const uint8_t esphome_host_mac_address[6] = USE_ESPHOME_HOST_MAC_ADDRESS;
  memcpy(mac, esphome_host_mac_address, sizeof(esphome_host_mac_address));
#elif defined(USE_ESP32)
#if defined(CONFIG_SOC_IEEE802154_SUPPORTED) || defined(USE_ESP32_IGNORE_EFUSE_MAC_CRC)
  // When CONFIG_SOC_IEEE802154_SUPPORTED is defined, esp_efuse_mac_get_default
  // returns the 802.15.4 EUI-64 address. Read directly from eFuse instead.
  // On some devices, the MAC address that is burnt into EFuse does not
  // match the CRC that goes along with it. For those devices, this
  // work-around reads and uses the MAC address as-is from EFuse,
  // without doing the CRC check.
  if (has_custom_mac_address()) {
    esp_efuse_read_field_blob(ESP_EFUSE_MAC_CUSTOM, mac, 48);
  } else {
    esp_efuse_read_field_blob(ESP_EFUSE_MAC_FACTORY, mac, 48);
  }
#else
  if (has_custom_mac_address()) {
    esp_efuse_mac_get_custom(mac);
  } else {
    esp_efuse_mac_get_default(mac);
  }
#endif
#elif defined(USE_ESP8266)
  wifi_get_macaddr(STATION_IF, mac);
#elif defined(USE_RP2040) && defined(USE_WIFI)
  WiFi.macAddress(mac);
#elif defined(USE_LIBRETINY)
  WiFi.macAddress(mac);
#else
// this should be an error, but that messes with CI checks. #error No mac address method defined
#endif
}

std::string get_mac_address() {
  uint8_t mac[6];
  get_mac_address_raw(mac);
  return str_snprintf("%02x%02x%02x%02x%02x%02x", 12, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

std::string get_mac_address_pretty() {
  uint8_t mac[6];
  get_mac_address_raw(mac);
  return str_snprintf("%02X:%02X:%02X:%02X:%02X:%02X", 17, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

#ifdef USE_ESP32
void set_mac_address(uint8_t *mac) { esp_base_mac_addr_set(mac); }
#endif

bool has_custom_mac_address() {
#if defined(USE_ESP32) && !defined(USE_ESP32_IGNORE_EFUSE_CUSTOM_MAC)
  uint8_t mac[6];
  // do not use 'esp_efuse_mac_get_custom(mac)' because it drops an error in the logs whenever it fails
#ifndef USE_ESP32_VARIANT_ESP32
  return (esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA_MAC_CUSTOM, mac, 48) == ESP_OK) && mac_address_is_valid(mac);
#else
  return (esp_efuse_read_field_blob(ESP_EFUSE_MAC_CUSTOM, mac, 48) == ESP_OK) && mac_address_is_valid(mac);
#endif
#else
  return false;
#endif
}

bool mac_address_is_valid(const uint8_t *mac) {
  bool is_all_zeros = true;
  bool is_all_ones = true;

  for (uint8_t i = 0; i < 6; i++) {
    if (mac[i] != 0) {
      is_all_zeros = false;
    }
  }
  for (uint8_t i = 0; i < 6; i++) {
    if (mac[i] != 0xFF) {
      is_all_ones = false;
    }
  }
  return !(is_all_zeros || is_all_ones);
}

void delay_microseconds_safe(uint32_t us) {  // avoids CPU locks that could trigger WDT or affect WiFi/BT stability
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
