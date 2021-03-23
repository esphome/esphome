#include "buffer_565.h"

namespace esphome {
namespace display {
#ifdef USE_BUFFER_RGB565
static const char *TAG = "buffer_565";

bool Bufferex565::init_buffer(int width, int height) {
  this->width_ = width;
  this->height_ = height;

  this->buffer_ = new_buffer<uint16_t>(this->get_buffer_length());
  if (this->buffer_ == nullptr) {
    return false;
  }
  memset(this->buffer_, 0x00, this->get_buffer_size());
  return true;
}

void HOT Bufferex565::fill_buffer(Color color) {
  display::BufferexBase::fill_buffer(color);

  auto color565 = ColorUtil::color_to_565(color);
  memset(this->buffer_, color565, this->get_buffer_size());
}

size_t HOT Bufferex565::get_buffer_size() { return this->get_buffer_length() * 2; }

bool HOT Bufferex565::set_buffer(int x, int y, Color color) {
  const uint16_t color565 = ColorUtil::color_to_565(color);
  uint32_t pos = get_pixel_buffer_position_(x, y);
  if (this->buffer_[pos] != color565) {
    this->buffer_[pos] = color565;
    return true;
  }
  return false;
}

// 565
uint16_t HOT Bufferex565::get_pixel_to_565(uint32_t pos) { return this->buffer_[pos]; }

// 666
uint32_t HOT Bufferex565::get_pixel_to_666(uint32_t pos) {
  return ColorUtil::color_to_666(
      ColorUtil::to_color(this->buffer_[pos], ColorOrder::COLOR_ORDER_RGB, ColorBitness::COLOR_BITNESS_332, true),
      this->driver_right_bit_aligned_);
}
#endif
}  // namespace display
}  // namespace esphome
