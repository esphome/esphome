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

  /// Retrieve the hash as hex characters
  virtual void get_hex(char *output) = 0;

  /// Compare the hash against a provided byte-encoded hash
  virtual bool equals_bytes(const uint8_t *expected) = 0;

  /// Compare the hash against a provided hex-encoded hash
  bool equals_hex(const char *expected) {
    const size_t hash_bytes = this->get_hex_size() / 2;
    uint8_t parsed[32];  // Max size for SHA256
    if (!parse_hex(expected, parsed, hash_bytes)) {
      return false;
    }
    return this->equals_bytes(parsed);
  }

  /// Get the size of the hex output (32 for MD5, 64 for SHA256)
  virtual size_t get_hex_size() const = 0;
};

}  // namespace esphome
