#include "esphome/core/helpers.h"
#include <cstdio>
#include <algorithm>

#ifdef ARDUINO_ARCH_ESP8266
#include <ESP8266WiFi.h>
#else
#include <Esp.h>
#endif

#include "esphome/core/log.h"
#include "esphome/core/esphal.h"

namespace esphome {

static const char *const TAG = "helpers";

std::string get_mac_address() {
  char tmp[20];
  uint8_t mac[6];
#ifdef ARDUINO_ARCH_ESP32
  esp_efuse_mac_get_default(mac);
#endif
#ifdef ARDUINO_ARCH_ESP8266
  WiFi.macAddress(mac);
#endif
  sprintf(tmp, "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return std::string(tmp);
}

std::string get_mac_address_pretty() {
  char tmp[20];
  uint8_t mac[6];
#ifdef ARDUINO_ARCH_ESP32
  esp_efuse_mac_get_default(mac);
#endif
#ifdef ARDUINO_ARCH_ESP8266
  WiFi.macAddress(mac);
#endif
  sprintf(tmp, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return std::string(tmp);
}

std::string generate_hostname(const std::string &base) { return base + std::string("-") + get_mac_address(); }

uint32_t random_uint32() {
#ifdef ARDUINO_ARCH_ESP32
  return esp_random();
#else
  return os_random();
#endif
}

double random_double() { return random_uint32() / double(UINT32_MAX); }

float random_float() { return float(random_double()); }

void fill_random(uint8_t *data, size_t len) {
#ifdef ARDUINO_ARCH_ESP32
  esp_fill_random(data, len);
#else
  int err = os_get_random(data, len);
  assert(err == 0);
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
  uint8_t rand32 = fast_random_32();
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

std::string to_lowercase_underscore(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), ::tolower);
  std::replace(s.begin(), s.end(), ' ', '_');
  return s;
}

std::string sanitize_string_allowlist(const std::string &s, const std::string &allowlist) {
  std::string out(s);
  out.erase(std::remove_if(out.begin(), out.end(),
                           [&allowlist](const char &c) { return allowlist.find(c) == std::string::npos; }),
            out.end());
  return out;
}

std::string sanitize_hostname(const std::string &hostname) {
  std::string s = sanitize_string_allowlist(hostname, HOSTNAME_CHARACTER_ALLOWLIST);
  return truncate_string(s, 63);
}

std::string truncate_string(const std::string &s, size_t length) {
  if (s.length() > length)
    return s.substr(0, length);
  return s;
}

std::string value_accuracy_to_string(float value, int8_t accuracy_decimals) {
  auto multiplier = float(powf(10.0f, accuracy_decimals));
  float value_rounded = roundf(value * multiplier) / multiplier;
  char tmp[32];  // should be enough, but we should maybe improve this at some point.
  dtostrf(value_rounded, 0, uint8_t(std::max(0, int(accuracy_decimals))), tmp);
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
static char *global_json_build_buffer = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static size_t global_json_build_buffer_size = 0;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void reserve_global_json_build_buffer(size_t required_size) {
  if (global_json_build_buffer_size == 0 || global_json_build_buffer_size < required_size) {
    delete[] global_json_build_buffer;
    global_json_build_buffer_size = std::max(required_size, global_json_build_buffer_size * 2);

    size_t remainder = global_json_build_buffer_size % 16U;
    if (remainder != 0)
      global_json_build_buffer_size += 16 - remainder;

    global_json_build_buffer = new char[global_json_build_buffer_size];
  }
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

const char *const HOSTNAME_CHARACTER_ALLOWLIST = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_";

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

void delay_microseconds_accurate(uint32_t usec) {
  if (usec == 0)
    return;
  if (usec < 5000UL) {
    delayMicroseconds(usec);
    return;
  }
  uint32_t start = micros();
  while (micros() - start < usec) {
    delay(0);
  }
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
optional<float> parse_float(const std::string &str) {
  char *end;
  float value = ::strtof(str.c_str(), &end);
  if (end == nullptr || end != str.end().base())
    return {};
  return value;
}
optional<int> parse_int(const std::string &str) {
  char *end;
  int value = ::strtol(str.c_str(), &end, 10);
  if (end == nullptr || end != str.end().base())
    return {};
  return value;
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
template float clamp(float, float, float);
template int clamp(int, int, int);

float lerp(float completion, float start, float end) { return start + (end - start) * completion; }

bool str_startswith(const std::string &full, const std::string &start) { return full.rfind(start, 0) == 0; }
bool str_endswith(const std::string &full, const std::string &ending) {
  return full.rfind(ending) == (full.size() - ending.size());
}

uint16_t encode_uint16(uint8_t msb, uint8_t lsb) { return (uint16_t(msb) << 8) | uint16_t(lsb); }
std::array<uint8_t, 2> decode_uint16(uint16_t value) {
  uint8_t msb = (value >> 8) & 0xFF;
  uint8_t lsb = (value >> 0) & 0xFF;
  return {msb, lsb};
}

uint32_t encode_uint32(uint8_t msb, uint8_t byte2, uint8_t byte3, uint8_t lsb) {
  return (uint32_t(msb) << 24) | (uint32_t(byte2) << 16) | (uint32_t(byte3) << 8) | uint32_t(lsb);
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

#ifdef ARDUINO_ARCH_ESP8266
ICACHE_RAM_ATTR InterruptLock::InterruptLock() { xt_state_ = xt_rsil(15); }
ICACHE_RAM_ATTR InterruptLock::~InterruptLock() { xt_wsr_ps(xt_state_); }
#endif
#ifdef ARDUINO_ARCH_ESP32
ICACHE_RAM_ATTR InterruptLock::InterruptLock() { portDISABLE_INTERRUPTS(); }
ICACHE_RAM_ATTR InterruptLock::~InterruptLock() { portENABLE_INTERRUPTS(); }
#endif

}  // namespace esphome
