#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include "esphome/core/helpers.h"

namespace esphome {

/// Base class for hash algorithms
class HashBase {
 public:
  virtual ~HashBase() = default;

  /// Initialize a new hash computation
  virtual void init() = 0;

  /// Add bytes of data for the hash
  virtual void add(const uint8_t *data, size_t len) = 0;
  void add(const char *data, size_t len) { this->add((const uint8_t *) data, len); }

  /// Compute the hash based on provided data
  virtual void calculate() = 0;

  /// Retrieve the hash as bytes
  void get_bytes(uint8_t *output) { memcpy(output, this->digest_, this->get_size()); }

  /// Retrieve the hash as hex characters
  void get_hex(char *output) {
    for (size_t i = 0; i < this->get_size(); i++) {
      uint8_t byte = this->digest_[i];
      output[i * 2] = format_hex_char(byte >> 4);
      output[i * 2 + 1] = format_hex_char(byte & 0x0F);
    }
  }

  /// Compare the hash against a provided byte-encoded hash
  bool equals_bytes(const uint8_t *expected) { return memcmp(this->digest_, expected, this->get_size()) == 0; }

  /// Compare the hash against a provided hex-encoded hash
  bool equals_hex(const char *expected) {
    uint8_t parsed[32];  // Fixed size for max hash (SHA256 = 32 bytes)
    if (!parse_hex(expected, parsed, this->get_size())) {
      return false;
    }
    return this->equals_bytes(parsed);
  }

  /// Get the size of the hash in bytes (16 for MD5, 32 for SHA256)
  virtual size_t get_size() const = 0;

 protected:
  uint8_t digest_[32];  // Storage sized for max(MD5=16, SHA256=32) bytes
};

}  // namespace esphome
