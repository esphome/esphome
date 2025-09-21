#pragma once

#include <cstdint>
#include <cstddef>

namespace esphome {

/// Base class for hash algorithms
class HashBase {
 public:
  virtual ~HashBase() = default;

  /// Initialize a new hash computation
  virtual void init() = 0;

  /// Add bytes of data for the hash
  virtual void add(const uint8_t *data, size_t len) = 0;
  virtual void add(const char *data, size_t len) { this->add((const uint8_t *) data, len); }

  /// Compute the hash based on provided data
  virtual void calculate() = 0;

  /// Retrieve the hash as hex characters
  virtual void get_hex(char *output) = 0;

  /// Get the size of the hex output (32 for MD5, 64 for SHA256)
  virtual size_t get_hex_size() const = 0;

  /// Get the algorithm name for logging
  virtual const char *get_name() const = 0;
};

}  // namespace esphome
