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
  void add(const std::string &data) { this->add((const uint8_t *) data.c_str(), data.length()); }

  void calculate() override;

  std::string get_hex_string();

  /// Get the size of the hex output (64 for SHA256)
  size_t get_hex_size() const override { return 64; }

  bool equals_hex(const char *expected);

 protected:
#if defined(USE_ESP32) || defined(USE_LIBRETINY)
  mbedtls_sha256_context ctx_{};
#elif defined(USE_ESP8266) || defined(USE_RP2040)
  br_sha256_context ctx_{};
  bool calculated_{false};
#elif defined(USE_HOST)
  EVP_MD_CTX *ctx_{nullptr};
  bool calculated_{false};
#else
#error "SHA256 not supported on this platform"
#endif
};

}  // namespace esphome::sha256

#endif  // Platform check
