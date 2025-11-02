#pragma once

#include "buffer.h"

#include <cstddef>

namespace esphome::camera {

/// Interface for a buffer supporting resizing and variable-length data.
class DynamicBuffer : public Buffer {
 public:
  /// Sets logical buffer size, reallocates if needed.
  /// Returns true on success, false on allocation failure.
  virtual bool set_buffer_size(size_t size) = 0;

  /// Returns total allocated buffer size.
  virtual size_t get_max_size() const = 0;
};

}  // namespace esphome::camera
