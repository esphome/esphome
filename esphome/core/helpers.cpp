#include "esphome/core/helpers.h"
#include "esphome/core/defines.h"
#include <cstdio>
#include <algorithm>
#include <cmath>
#include <cstring>

#if defined(USE_ESP8266)
#include <osapi.h>
#include <user_interface.h>
// for xt_rsil()/xt_wsr_ps()
#include <Arduino.h>
#elif defined(USE_ESP32_FRAMEWORK_ARDUINO)
#include <Esp.h>
#elif defined(USE_ESP_IDF)
#include "esp_system.h"
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#endif
#ifdef USE_ESP32_IGNORE_EFUSE_MAC_CRC
#include "esp_efuse.h"
#include "esp_efuse_table.h"
#endif

#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {

static const char *const TAG = "helpers";

void get_mac_address_raw(uint8_t *mac) {
#if defined(USE_ESP32)
#if defined(USE_ESP32_IGNORE_EFUSE_MAC_CRC)
  // On some devices, the MAC address that is burnt into EFuse does not
  // match the CRC that goes along with it. For those devices, this
  // work-around reads and uses the MAC address as-is from EFuse,
  // without doing the CRC check.
  esp_efuse_read_field_blob(ESP_EFUSE_MAC_FACTORY, mac, 48);
#else
  esp_efuse_mac_get_default(mac);
#endif
#elif defined(USE_ESP8266)
  wifi_get_macaddr(STATION_IF, mac);
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

std::string generate_hostname(const std::string &base) { return base + std::string("-") + get_mac_address(); }

uint32_t random_uint32() {
#ifdef USE_ESP32
  return esp_random();
#elif defined(USE_ESP8266)
  return os_random();
#endif
}

double random_double() { return random_uint32() / double(UINT32_MAX); }

float random_float() { return float(random_double()); }

void fill_random(uint8_t *data, size_t len) {
#if defined(USE_ESP_IDF) || defined(USE_ESP32_FRAMEWORK_ARDUINO)
  esp_fill_random(data, len);
#elif defined(USE_ESP8266)
  int err = os_get_random(data, len);
  assert(err == 0);
#else
#error "No random source for this system config"
#endif
}

static uint32_t fast_random_seed = 0;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void fast_random_set_seed(uint32_t seed) { fast_random_seed = seed; }
uint32_t fast_random_32() {
  fast_random_seed = (fast_random_seed * 2654435769ULL) + 40503ULL;
  return fast_random_seed;
}
uint16_t fast_random_16() {
  uint32_t rand32 = fast_random_32();
  return (rand32 & 0xFFFF) + (rand32 >> 16);
}
uint8_t fast_random_8() {
  uint32_t rand32 = fast_random_32();
  return (rand32 & 0xFF) + ((rand32 >> 8) & 0xFF);
}

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
std::string uint64_to_string(uint64_t num) {
  char buffer[17];
  auto *address16 = reinterpret_cast<uint16_t *>(&num);
  snprintf(buffer, sizeof(buffer), "%04X%04X%04X%04X", address16[3], address16[2], address16[1], address16[0]);
  return std::string(buffer);
}
std::string uint32_to_string(uint32_t num) {
  char buffer[9];
  auto *address16 = reinterpret_cast<uint16_t *>(&num);
  snprintf(buffer, sizeof(buffer), "%04X%04X", address16[1], address16[0]);
  return std::string(buffer);
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

uint8_t crc8(uint8_t *data, uint8_t len) {
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

void delay_microseconds_safe(uint32_t us) {  // avoids CPU locks that could trigger WDT or affect WiFi/BT stability
  auto start = micros();
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

uint8_t reverse_bits_8(uint8_t x) {
  x = ((x & 0xAA) >> 1) | ((x & 0x55) << 1);
  x = ((x & 0xCC) >> 2) | ((x & 0x33) << 2);
  x = ((x & 0xF0) >> 4) | ((x & 0x0F) << 4);
  return x;
}

uint16_t reverse_bits_16(uint16_t x) {
  return uint16_t(reverse_bits_8(x & 0xFF) << 8) | uint16_t(reverse_bits_8(x >> 8));
}
std::string to_string(const std::string &val) { return val; }
std::string to_string(int val) {
  char buf[64];
  sprintf(buf, "%d", val);
  return buf;
}
std::string to_string(long val) {  // NOLINT
  char buf[64];
  sprintf(buf, "%ld", val);
  return buf;
}
std::string to_string(long long val) {  // NOLINT
  char buf[64];
  sprintf(buf, "%lld", val);
  return buf;
}
std::string to_string(unsigned val) {  // NOLINT
  char buf[64];
  sprintf(buf, "%u", val);
  return buf;
}
std::string to_string(unsigned long val) {  // NOLINT
  char buf[64];
  sprintf(buf, "%lu", val);
  return buf;
}
std::string to_string(unsigned long long val) {  // NOLINT
  char buf[64];
  sprintf(buf, "%llu", val);
  return buf;
}
std::string to_string(float val) {
  char buf[64];
  sprintf(buf, "%f", val);
  return buf;
}
std::string to_string(double val) {
  char buf[64];
  sprintf(buf, "%f", val);
  return buf;
}
std::string to_string(long double val) {
  char buf[64];
  sprintf(buf, "%Lf", val);
  return buf;
}

optional<int> parse_hex(const char chr) {
  int out = chr;
  if (out >= '0' && out <= '9')
    return (out - '0');
  if (out >= 'A' && out <= 'F')
    return (10 + (out - 'A'));
  if (out >= 'a' && out <= 'f')
    return (10 + (out - 'a'));
  return {};
}

optional<int> parse_hex(const std::string &str, size_t start, size_t length) {
  if (str.length() < start) {
    return {};
  }
  size_t end = start + length;
  if (str.length() < end) {
    return {};
  }
  int out = 0;
  for (size_t i = start; i < end; i++) {
    char chr = str[i];
    auto digit = parse_hex(chr);
    if (!digit.has_value()) {
      ESP_LOGW(TAG, "Can't convert '%s' to number, invalid character %c!", str.substr(start, length).c_str(), chr);
      return {};
    }
    out = (out << 4) | *digit;
  }
  return out;
}

uint32_t fnv1_hash(const std::string &str) {
  uint32_t hash = 2166136261UL;
  for (char c : str) {
    hash *= 16777619UL;
    hash ^= c;
  }
  return hash;
}
bool str_equals_case_insensitive(const std::string &a, const std::string &b) {
  return strcasecmp(a.c_str(), b.c_str()) == 0;
}

template<uint32_t> uint32_t reverse_bits(uint32_t x) {
  return uint32_t(reverse_bits_16(x & 0xFFFF) << 16) | uint32_t(reverse_bits_16(x >> 16));
}

static int high_freq_num_requests = 0;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void HighFrequencyLoopRequester::start() {
  if (this->started_)
    return;
  high_freq_num_requests++;
  this->started_ = true;
}
void HighFrequencyLoopRequester::stop() {
  if (!this->started_)
    return;
  high_freq_num_requests--;
  this->started_ = false;
}
bool HighFrequencyLoopRequester::is_high_frequency() { return high_freq_num_requests > 0; }

template<typename T> T clamp(const T val, const T min, const T max) {
  if (val < min)
    return min;
  if (val > max)
    return max;
  return val;
}
template uint8_t clamp(uint8_t, uint8_t, uint8_t);
template float clamp(float, float, float);
template int clamp(int, int, int);

float lerp(float completion, float start, float end) { return start + (end - start) * completion; }

bool str_startswith(const std::string &full, const std::string &start) { return full.rfind(start, 0) == 0; }
bool str_endswith(const std::string &full, const std::string &ending) {
  return full.rfind(ending) == (full.size() - ending.size());
}
std::string str_snprintf(const char *fmt, size_t length, ...) {
  std::string str;
  va_list args;

  str.resize(length);
  va_start(args, length);
  size_t out_length = vsnprintf(&str[0], length + 1, fmt, args);
  va_end(args);

  if (out_length < length)
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

std::string hexencode(const uint8_t *data, uint32_t len) {
  char buf[20];
  std::string res;
  for (size_t i = 0; i < len; i++) {
    if (i + 1 != len) {
      sprintf(buf, "%02X.", data[i]);
    } else {
      sprintf(buf, "%02X ", data[i]);
    }
    res += buf;
  }
  sprintf(buf, "(%u)", len);
  res += buf;
  return res;
}

void rgb_to_hsv(float red, float green, float blue, int &hue, float &saturation, float &value) {
  float max_color_value = std::max(std::max(red, green), blue);
  float min_color_value = std::min(std::min(red, green), blue);
  float delta = max_color_value - min_color_value;

  if (delta == 0)
    hue = 0;
  else if (max_color_value == red)
    hue = int(fmod(((60 * ((green - blue) / delta)) + 360), 360));
  else if (max_color_value == green)
    hue = int(fmod(((60 * ((blue - red) / delta)) + 120), 360));
  else if (max_color_value == blue)
    hue = int(fmod(((60 * ((red - green) / delta)) + 240), 360));

  if (max_color_value == 0)
    saturation = 0;
  else
    saturation = delta / max_color_value;

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

#ifdef USE_ESP8266
IRAM_ATTR InterruptLock::InterruptLock() { xt_state_ = xt_rsil(15); }
IRAM_ATTR InterruptLock::~InterruptLock() { xt_wsr_ps(xt_state_); }
#endif
#ifdef USE_ESP32
IRAM_ATTR InterruptLock::InterruptLock() { portDISABLE_INTERRUPTS(); }
IRAM_ATTR InterruptLock::~InterruptLock() { portENABLE_INTERRUPTS(); }
#endif

// ---------------------------------------------------------------------------------------------------------------------

std::string str_truncate(const std::string &str, size_t length) {
  return str.length() > length ? str.substr(0, length) : str;
}
std::string str_until(const char *str, char ch) {
  char *pos = strchr(str, ch);
  return pos == nullptr ? std::string(str) : std::string(str, pos - str);
}
std::string str_until(const std::string &str, char ch) { return str.substr(0, str.find(ch)); }
std::string str_snake_case(const std::string &str) {
  std::string result;
  result.resize(str.length());
  std::transform(str.begin(), str.end(), result.begin(), ::tolower);
  std::replace(result.begin(), result.end(), ' ', '_');
  return result;
}
std::string str_sanitize(const std::string &str) {
  std::string out;
  std::copy_if(str.begin(), str.end(), std::back_inserter(out), [](const char &c) {
    return c == '-' || c == '_' || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
  });
  return out;
}

}  // namespace esphome
