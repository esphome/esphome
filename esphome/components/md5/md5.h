#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP_IDF
#include "esp_rom_md5.h"
#define MD5_CTX_TYPE md5_context_t
#endif

#if defined(USE_ARDUINO) && defined(USE_ESP32)
#include "rom/md5_hash.h"
#define MD5_CTX_TYPE MD5Context
#endif

#if defined(USE_ARDUINO) && defined(USE_ESP8266)
#include <md5.h>
#define MD5_CTX_TYPE md5_context_t
#endif

#ifdef USE_RP2040
#include <MD5Builder.h>
#define MD5_CTX_TYPE br_md5_context
#endif

#if defined(USE_LIBRETINY)
#include <MD5.h>
#define MD5_CTX_TYPE LT_MD5_CTX_T
#endif

namespace esphome {
namespace md5 {

class MD5Digest {
 public:
  MD5Digest() = default;
  ~MD5Digest() = default;

  /// Initialize a new MD5 digest computation.
  void init();

  /// Add bytes of data for the digest.
  void add(const uint8_t *data, size_t len);
  void add(const char *data, size_t len) { this->add((const uint8_t *) data, len); }

  /// Compute the digest, based on the provided data.
  void calculate();

  /// Retrieve the MD5 digest as bytes.
  /// The output must be able to hold 16 bytes or more.
  void get_bytes(uint8_t *output);

  /// Retrieve the MD5 digest as hex characters.
  /// The output must be able to hold 32 bytes or more.
  void get_hex(char *output);

  /// Compare the digest against a provided byte-encoded digest (16 bytes).
  bool equals_bytes(const uint8_t *expected);

  /// Compare the digest against a provided hex-encoded digest (32 bytes).
  bool equals_hex(const char *expected);

 protected:
  MD5_CTX_TYPE ctx_{};
  uint8_t digest_[16];
};

}  // namespace md5
}  // namespace esphome
