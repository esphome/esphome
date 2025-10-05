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
  using HashBase::add;  // Bring base class overload into scope
  void add(const std::string &data) { this->add((const uint8_t *) data.c_str(), data.length()); }

  void calculate() override;

  /// Get the size of the hash in bytes (32 for SHA256)
  size_t get_size() const override { return 32; }

 protected:
#if defined(USE_ESP32) || defined(USE_LIBRETINY)
  // CRITICAL: The mbedtls context MUST be stack-allocated (not a pointer) for ESP32-S3 hardware SHA acceleration.
  // The ESP32-S3 DMA engine references this structure's memory addresses. If the context is passed to another
  // function (crossing stack frames) or if VLAs are present, the DMA operations will corrupt memory and produce
  // truncated/incorrect hash results.
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
