#include "bufferex_332.h"

namespace esphome {
namespace display {
static const char *TAG = "bufferex_332";

void Bufferex332::init_buffer(int width, int height) {
  this->width_ = width;
  this->height_ = height;

  this->buffer_ = new uint8_t[this->get_buffer_length()];
  if (this->buffer_ == nullptr) {
    ESP_LOGE(TAG, "Could not allocate buffer for display!");
    return;
  }
  memset(this->buffer_, 0x00, this->get_buffer_size());
}

void Bufferex332::fill_buffer(Color color) {
  display::BufferexBase::fill_buffer(color);

  auto color332 = ColorUtil::color_to_332(color);
  ESP_LOGD(TAG, "fill_buffer color: %d", color332);
  memset(this->buffer_, color332, this->get_buffer_size());
}

void HOT Bufferex332::set_buffer(int x, int y, Color color) {
  uint32_t pos = get_pixel_buffer_position_(x, y);
  const uint16_t color332 = ColorUtil::color_to_332(color);
  this->buffer_[pos] = color332;
}

size_t Bufferex332::get_buffer_size() { return this->get_buffer_length(); }

// 565
uint16_t Bufferex332::get_pixel_to_565(uint32_t pos) {
  return ColorUtil::color_to_565(
      ColorUtil::to_color(this->buffer_[pos], ColorOrder::COLOR_ORDER_RGB, ColorBitness::COLOR_BITNESS_332, true));
}

// 666
uint32_t Bufferex332::get_pixel_to_666(uint32_t pos) {
  return ColorUtil::color_to_666(
      ColorUtil::to_color(this->buffer_[pos], ColorOrder::COLOR_ORDER_RGB, ColorBitness::COLOR_BITNESS_332, true),
      this->driver_right_bit_aligned_);
}

}  // namespace display
}  // namespace esphome
