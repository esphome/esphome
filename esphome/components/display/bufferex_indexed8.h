#pragma once
#include "bufferex_base.h"

namespace esphome {
namespace display {

class BufferexIndexed8 : public display::BufferexBase {
 public:
  uint8_t *buffer_{nullptr};

  void init_buffer(int width, int height) override;
  void set_buffer(int x, int y, Color color) override;

  void fill_buffer(Color color) override;

  uint8_t get_pixel_value(uint32_t pos) override;
  uint16_t get_pixel_to_565(uint32_t pos) override;
  uint32_t get_pixel_to_666(uint32_t pos) override;

  size_t get_buffer_length() override;
  size_t get_buffer_size() override;

  void set_color_off(Color color) { color_off_ = color; }
  void set_color_on(Color color) { color_on_ = color; }

 protected:
  Color color_on_ = COLOR_WHITE;
  Color color_off_ = COLOR_BLACK;

  size_t get_buffer_length_ = 0;

  uint8_t get_index_value_(int x, int y);
  uint8_t get_index_value_(uint32_t pos);
  display::BufferType get_buffer_type() override { return this->buffer_type_; }
  uint8_t get_pixel_storage_size() override { return this->pixel_storage_size_; }
  uint8_t get_index_from_color_(Color color);
  uint8_t get_value_from_color_index_(uint8_t index);

  display::BufferType buffer_type_ = display::BufferType::BUFFER_TYPE_INDEXED;
  uint8_t pixel_storage_size_ = 1;
};  // class Bufferex332
}  // namespace display
}  // namespace esphome
