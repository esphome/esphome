#pragma once
#include "bufferex_base.h"

namespace esphome {
namespace display {
#ifdef USE_BUFFER_RGB666
class Bufferex666 : public display::BufferexBase {
 public:
  uint32_t *buffer_{nullptr};

  bool init_buffer(int width, int height) override;
  bool set_buffer(int x, int y, Color color) override;
  void fill_buffer(Color color) override;
  uint16_t get_pixel_to_565(uint32_t pos) override;
  uint32_t get_pixel_to_666(uint32_t pos) override;
  size_t get_buffer_size() override;
  display::BufferType get_buffer_type() override { return this->buffer_type_; }
  uint8_t get_pixel_storage_size() override { return this->pixel_storage_size_; }

 protected:
  display::BufferType buffer_type_ = display::BufferType::BUFFER_TYPE_666;
  uint8_t pixel_storage_size_ = 32;
};  // class Bufferex666
#endif
}  // namespace display
}  // namespace esphome
