#pragma once

#include "esphome/core/defines.h"
#include "esphome/components/md5/md5.h"

#include <string>

namespace esphome {
namespace hmac_md5 {

class HmacMD5 {
 public:
  HmacMD5() = default;
  ~HmacMD5() = default;

  /// Initialize a new MD5 digest computation.
  void init(const uint8_t *key, size_t len);
  void init(const char *key, size_t len) { this->init((const uint8_t *) key, len); }
  void init(const std::string &key) { this->init(key.c_str(), key.length()); }

  /// Add bytes of data for the digest.
  void add(const uint8_t *data, size_t len);
  void add(const char *data, size_t len) { this->add((const uint8_t *) data, len); }

  /// Compute the digest, based on the provided data.
  void calculate();

  /// Retrieve the HMAC-MD5 digest as bytes.
  /// The output must be able to hold 16 bytes or more.
  void get_bytes(uint8_t *output);

  /// Retrieve the HMAC-MD5 digest as hex characters.
  /// The output must be able to hold 32 bytes or more.
  void get_hex(char *output);

  /// Compare the digest against a provided byte-encoded digest (16 bytes).
  bool equals_bytes(const uint8_t *expected);

  /// Compare the digest against a provided hex-encoded digest (32 bytes).
  bool equals_hex(const char *expected);

 protected:
  md5::MD5Digest ihash_;
  md5::MD5Digest ohash_;
};

}  // namespace hmac_md5
}  // namespace esphome
