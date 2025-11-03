#pragma once

#include "buffer.h"
#include "camera.h"
#include "dynamic_buffer.h"
#include "ram_allocator_cache_aligned.h"

namespace esphome::camera {

/// Default implementation of Buffer Interface.
/// Uses a RAMAllocator for memory reservation.
class BufferImpl : public DynamicBuffer {
 public:
  BufferImpl();
  explicit BufferImpl(size_t size);
  explicit BufferImpl(CameraImageSpec *spec);
  // -------- Buffer --------
  uint8_t *get_data() const override { return data_; }
  size_t get_size() const override { return size_; }
  // --- DynamicBuffer  ---
  bool set_buffer_size(size_t size) override;
  size_t get_max_size() const override { return this->capacity_; }
  // ------------------------
  ~BufferImpl() override;

 protected:
  RAMAllocatorCacheAligned<uint8_t> allocator_;
  size_t capacity_{};
  size_t size_{};
  uint8_t *data_{};
};

}  // namespace esphome::camera
