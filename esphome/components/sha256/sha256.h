#pragma once

#include "esphome/core/defines.h"

// Only define SHA256 on platforms that support it
#if defined(USE_ESP32) || defined(USE_ESP8266) || defined(USE_RP2040) || defined(USE_LIBRETINY) || defined(USE_HOST)

#include <cstdint>
#include <string>
#include <memory>
#include "esphome/core/hash_base.h"

#if defined(USE_ESP32)
#include <esp_idf_version.h>
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
// mbedtls 4.0 (IDF 6.0) removed the legacy mbedtls_sha256_* API.
// Use the PSA Crypto API instead. PSA crypto is auto-initialized by
// ESP-IDF at startup (esp_psa_crypto_init.c, priority 104).
#define USE_SHA256_PSA
#include <psa/crypto.h>
#else
#define USE_SHA256_MBEDTLS
#include "mbedtls/sha256.h"
#endif
#elif defined(USE_LIBRETINY)
#define USE_SHA256_MBEDTLS
#include "mbedtls/sha256.h"
#elif defined(USE_ESP8266) || defined(USE_RP2040)
#include <bearssl/bearssl_hash.h>
#elif defined(USE_HOST)
#include <openssl/evp.h>
#else
#error "SHA256 not supported on this platform"
#endif

namespace esphome::sha256 {

/// SHA256 hash implementation.
///
/// CRITICAL for ESP32 variants (except original) with IDF 5.5.x hardware SHA acceleration:
/// 1. The object MUST stay in the same stack frame (no passing to other functions)
/// 2. NO Variable Length Arrays (VLAs) in the same function
///
/// Note: Alignment is handled automatically via the HashBase::digest_ member.
///
/// Example usage:
///   sha256::SHA256 hasher;
///   hasher.init();
///   hasher.add(data, len);
///   hasher.calculate();
class SHA256 : public esphome::HashBase {
 public:
  SHA256() = default;
  ~SHA256() override;

  void init() override;
  void add(const uint8_t *data, size_t len) override;
  using HashBase::add;  // Bring base class overload into scope
  void add(const std::string &data) { this->add((const uint8_t *) data.c_str(), data.length()); }

  void calculate() override;

  /// Get the size of the hash in bytes (32 for SHA256)
  size_t get_size() const override { return 32; }

 protected:
#if defined(USE_SHA256_PSA)
  psa_hash_operation_t op_ = PSA_HASH_OPERATION_INIT;
#elif defined(USE_SHA256_MBEDTLS)
  // The mbedtls context for ESP32-S3 hardware SHA requires proper alignment and stack frame constraints.
  // See class documentation above for critical requirements.
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
