#pragma once

#include "esphome/core/defines.h"

// Only define SHA256 on platforms that support it
#if defined(USE_ESP32) || defined(USE_ESP8266) || defined(USE_RP2040) || defined(USE_LIBRETINY) || defined(USE_HOST)

#include <cstdint>
#include <string>
#include <memory>
#include "esphome/core/hash_base.h"

#if defined(USE_ESP32) || defined(USE_LIBRETINY)
#include "mbedtls/sha256.h"
#elif defined(USE_ESP8266) || defined(USE_RP2040)
#include <bearssl/bearssl_hash.h>
#elif defined(USE_HOST)
#include <openssl/evp.h>
#else
#error "SHA256 not supported on this platform"
#endif

namespace esphome::sha256 {

class SHA256 : public esphome::HashBase {
 public:
  SHA256() = default;
  ~SHA256() override;

  void init() override;
  void add(const uint8_t *data, size_t len) override;
  void add(const char *data, size_t len) override { this->add((const uint8_t *) data, len); }
  void add(const std::string &data) { this->add(data.c_str(), data.length()); }

  void calculate() override;

  void get_bytes(uint8_t *output);
  void get_hex(char *output) override;
  std::string get_hex_string();

  /// Get the size of the hex output (64 for SHA256)
  size_t get_hex_size() const override { return 64; }

  /// Get the algorithm name for logging
  const char *get_name() const override { return "SHA256"; }

  bool equals_bytes(const uint8_t *expected);
  bool equals_hex(const char *expected);

 protected:
#if defined(USE_ESP32) || defined(USE_LIBRETINY)
  struct SHA256Context {
    mbedtls_sha256_context ctx;
    uint8_t hash[32];
  };
#elif defined(USE_ESP8266) || defined(USE_RP2040)
  struct SHA256Context {
    br_sha256_context ctx;
    uint8_t hash[32];
    bool calculated{false};
  };
#elif defined(USE_HOST)
  struct SHA256Context {
    EVP_MD_CTX *ctx{nullptr};
    uint8_t hash[32];
    bool calculated{false};
  };
#else
#error "SHA256 not supported on this platform"
#endif
  std::unique_ptr<SHA256Context> ctx_;
};

}  // namespace esphome::sha256

#endif  // Platform check
