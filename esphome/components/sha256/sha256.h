#pragma once

#include "esphome/core/defines.h"
#include <cstdint>
#include <string>
#include <memory>

#ifdef USE_ESP32
#include "mbedtls/sha256.h"
#elif defined(USE_ARDUINO)
#include <SHA256.h>
#endif

namespace esphome::sha256 {

class SHA256 {
 public:
  SHA256() = default;
  ~SHA256();

  void init();
  void add(const uint8_t *data, size_t len);
  void add(const char *data, size_t len) { this->add((const uint8_t *) data, len); }
  void add(const std::string &data) { this->add(data.c_str(), data.length()); }

  void calculate();

  void get_bytes(uint8_t *output);
  void get_hex(char *output);
  std::string get_hex_string();

  bool equals_bytes(const uint8_t *expected);
  bool equals_hex(const char *expected);

 protected:
#ifdef USE_ESP32
  struct SHA256Context {
    mbedtls_sha256_context ctx;
    uint8_t hash[32];
  };
#elif defined(USE_ARDUINO)
  struct SHA256Context {
    ::SHA256 sha;
    uint8_t hash[32];
    bool calculated{false};
  };
#else
#error "SHA256 not supported on this platform"
#endif
  std::unique_ptr<SHA256Context> ctx_;
};

}  // namespace esphome::sha256
