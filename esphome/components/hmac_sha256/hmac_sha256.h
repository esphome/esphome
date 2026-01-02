#pragma once

#include "esphome/core/defines.h"
#if defined(USE_ESP32) || defined(USE_ESP8266) || defined(USE_RP2040) || defined(USE_LIBRETINY) || defined(USE_HOST)

#include <string>

#if defined(USE_ESP32) || defined(USE_LIBRETINY)
#include "mbedtls/md.h"
#else
#include "esphome/components/sha256/sha256.h"
#endif

namespace esphome::hmac_sha256 {

class HmacSHA256 {
 public:
  HmacSHA256() = default;
  ~HmacSHA256();

  /// Initialize a new HMAC-SHA256 digest computation.
  void init(const uint8_t *key, size_t len);
  void init(const char *key, size_t len) { this->init((const uint8_t *) key, len); }
  void init(const std::string &key) { this->init(key.c_str(), key.length()); }

  /// Add bytes of data for the digest.
  void add(const uint8_t *data, size_t len);
  void add(const char *data, size_t len) { this->add((const uint8_t *) data, len); }

  /// Compute the digest, based on the provided data.
  void calculate();

  /// Retrieve the HMAC-SHA256 digest as bytes.
  /// The output must be able to hold 32 bytes or more.
  void get_bytes(uint8_t *output);

  /// Retrieve the HMAC-SHA256 digest as hex characters.
  /// The output must be able to hold 65 bytes or more (64 hex chars + null terminator).
  void get_hex(char *output);

  /// Compare the digest against a provided byte-encoded digest (32 bytes).
  bool equals_bytes(const uint8_t *expected);

  /// Compare the digest against a provided hex-encoded digest (64 bytes).
  bool equals_hex(const char *expected);

 protected:
#if defined(USE_ESP32) || defined(USE_LIBRETINY)
  static constexpr size_t SHA256_DIGEST_SIZE = 32;
  mbedtls_md_context_t ctx_{};
  uint8_t digest_[SHA256_DIGEST_SIZE]{};
#else
  sha256::SHA256 ihash_;
  sha256::SHA256 ohash_;
#endif
};

}  // namespace esphome::hmac_sha256
#endif
