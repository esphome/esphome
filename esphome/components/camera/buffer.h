#pragma once

#include <cinttypes>
#include <cstddef>

namespace esphome::camera {

/// Interface for a generic buffer that stores image data.
class Buffer {
 public:
  /// Returns a pointer to the buffer's data.
  virtual uint8_t *get_data_buffer() = 0;
  /// Returns the length of the buffer in bytes.
  virtual size_t get_data_length() = 0;
  virtual ~Buffer() = default;
};

}  // namespace esphome::camera
