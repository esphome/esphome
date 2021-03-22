#include "buffer_666.h"

namespace esphome {
namespace display {
#ifdef USE_BUFFER_RGB666
static const char *TAG = "buffer_666";

bool Bufferex666::init_buffer(int width, int height) {
  if (this->buffer_ != nullptr) {
    return true;
  }
  this->width_ = width;
  this->height_ = height;

  this->buffer_ = new_buffer<uint32_t>(this->get_buffer_length());
  if (this->buffer_ == nullptr) {
    return false;
  }

  memset(this->buffer_, 0x00, this->get_buffer_size());
  return true;
}

void Bufferex666::fill_buffer(Color color) {
  display::BufferexBase::fill_buffer(color);

  auto color666 = ColorUtil::color_to_666(color, this->driver_right_bit_aligned_);
  ESP_LOGD(TAG, "fill_buffer color: %d", color666);
  memset(this->buffer_, color666, this->get_buffer_size());
}
size_t Bufferex666::get_buffer_size() { return this->get_buffer_length() * 4; }

void HOT Bufferex666::set_buffer(int x, int y, Color color) {
  uint32_t pos = get_pixel_buffer_position_(x, y);
  const uint32_t color666 = ColorUtil::color_to_666(color, this->driver_right_bit_aligned_);
  if (this->buffer_[pos] != color565) {
    this->buffer_[pos] = color666;
    return true;
  }
  return false;
}

// 565
uint16_t Bufferex666::get_pixel_to_565(uint32_t pos) {
  return ColorUtil::color_to_565(ColorUtil::to_color(this->buffer_[pos], ColorOrder::COLOR_ORDER_RGB,
                                                     ColorBitness::COLOR_BITNESS_666, this->driver_right_bit_aligned_));
}

// 666
uint32_t Bufferex666::get_pixel_to_666(uint32_t pos) { return this->buffer_[pos]; }
#endif
}  // namespace display
}  // namespace esphome
