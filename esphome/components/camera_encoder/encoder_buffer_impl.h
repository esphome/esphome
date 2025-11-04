#pragma once

#include "esphome/components/camera/encoder.h"
#include "esphome/core/helpers.h"

namespace esphome::camera_encoder {

class EncoderBufferImpl : public camera::EncoderBuffer {
 public:
  // --- EncoderBuffer  ---
  bool set_buffer_size(size_t size) override;
  uint8_t *get_data() const override { return this->data_; }
  size_t get_size() const override { return this->size_; }
  size_t get_max_size() const override { return this->capacity_; }
  // ----------------------
  ~EncoderBufferImpl() override;

 protected:
  RAMAllocator<uint8_t> allocator_;
  size_t capacity_{};
  size_t size_{};
  uint8_t *data_{};
};

}  // namespace esphome::camera_encoder
