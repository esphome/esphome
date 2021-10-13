#pragma once

#include "esphome/core/defines.h"
#ifdef USE_ESP_IDF
#include "esp32/rom/md5_hash.h"
#endif
#ifdef USE_ARDUINO
#include "rom/md5_hash.h"
#endif

namespace esphome {

class MD5Digest {
 public:
  MD5Digest() = default;
  ~MD5Digest() = default;

  /// Initialize a new MD5 digest computation.
  void init();

  /// Add bytes of data for the digest.
  void add(uint8_t *data, size_t len);

  /// Compute the digest, based on the provided data.
  void calculate();

  /// Retrieve the MD5 digest as bytes.
  /// The output must be able to hold 16 bytes or more.
  void get_bytes(uint8_t *output);

  /// Retrieve the MD5 digest as hex characters.
  /// The output must be able to hold 32 bytes or more.
  void get_hex(char *output);

  /// Compare the digest against a provided byte-encoded digest (16 bytes).
  bool equals_bytes(const char *expected);

  /// Compare the digest against a provided hex-encoded digest (32 bytes).
  bool equals_hex(const char *expected);

 protected:
  MD5Context ctx_{};
  uint8_t digest_[16];
};

}  // namespace esphome
