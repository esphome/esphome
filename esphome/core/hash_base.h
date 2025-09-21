#pragma once

#include <cstdint>
#include <cstddef>
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
  virtual void get_hex(char *output) {
    const size_t hash_bytes = this->get_hex_size() / 2;
    for (size_t i = 0; i < hash_bytes; i++) {
      uint8_t byte = this->digest_[i];
      output[i * 2] = format_hex_char(byte >> 4);
      output[i * 2 + 1] = format_hex_char(byte & 0x0F);
    }
  }

  /// Get the size of the hex output (32 for MD5, 64 for SHA256)
  virtual size_t get_hex_size() const = 0;

 protected:
  uint8_t digest_[32];  // Common digest storage (MD5 uses 16 bytes, SHA256 uses 32)
};

}  // namespace esphome
