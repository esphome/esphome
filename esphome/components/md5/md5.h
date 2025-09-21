#pragma once

#include "esphome/core/defines.h"
#ifdef USE_MD5

#include "esphome/core/hash_base.h"

#ifdef USE_ESP32
#include "esp_rom_md5.h"
#define MD5_CTX_TYPE md5_context_t
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

class MD5Digest : public HashBase {
 public:
  MD5Digest() = default;
  ~MD5Digest() override = default;

  /// Initialize a new MD5 digest computation.
  void init() override;

  /// Add bytes of data for the digest.
  void add(const uint8_t *data, size_t len) override;

  /// Compute the digest, based on the provided data.
  void calculate() override;

  /// Retrieve the MD5 digest as bytes.
  /// The output must be able to hold 16 bytes or more.
  void get_bytes(uint8_t *output);

  /// Retrieve the MD5 digest as hex characters.
  /// The output must be able to hold 32 bytes or more.
  void get_hex(char *output) override;

  /// Get the size of the hex output (32 for MD5)
  size_t get_hex_size() const override { return 32; }

  /// Compare the digest against a provided byte-encoded digest (16 bytes)
  bool equals_bytes(const uint8_t *expected) override;

 protected:
  MD5_CTX_TYPE ctx_{};
  uint8_t digest_[16];
};

}  // namespace md5
}  // namespace esphome
#endif
