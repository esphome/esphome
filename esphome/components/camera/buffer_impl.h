#pragma once

#include "buffer.h"
#include "camera.h"

namespace esphome::camera {

/// Default implementation of Buffer Interface.
/// Uses a RAMAllocator for memory reservation.
class BufferImpl : public Buffer {
 public:
  explicit BufferImpl(size_t size);
  explicit BufferImpl(CameraImageSpec *spec);
  // -------- Buffer --------
  uint8_t *get_data_buffer() override { return data_; }
  size_t get_data_length() override { return size_; }
  // ------------------------
  ~BufferImpl() override;

 protected:
  RAMAllocator<uint8_t> allocator_;
  size_t size_{};
  uint8_t *data_{};
};

}  // namespace esphome::camera
