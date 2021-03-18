#include "bufferex_1bit_2color.h"

namespace esphome {
namespace display {
static const char *TAG = "bufferex_1bit_2color";

bool Bufferex1bit2color::init_buffer(int width, int height) {
  this->width_ = width;
  this->height_ = height;

  this->buffer_ = new uint8_t[this->get_buffer_length()];
  if (this->buffer_ == nullptr) {
    ESP_LOGE(TAG, "Could not allocate buffer for display!");
    return false;
  }
  memset(this->buffer_, 0x00, this->get_buffer_size());
  return true;
}

void Bufferex1bit2color::fill_buffer(Color color) {
  display::BufferexBase::fill_buffer(color);
  ESP_LOGD(TAG, "fill_buffer color: %d", ColorUtil::color_to_565(color));
  memset(this->buffer_, color.r + color.b + color.g == 0 ? 0 : 1, this->get_buffer_size());
}

uint32_t Bufferex1bit2color::get_pixel_buffer_position_internal_(int x, int y) { return (x + y * this->width_); }

void HOT Bufferex1bit2color::set_buffer(int x, int y, Color color) {
  const uint32_t byte_location = get_pixel_buffer_position_internal_(x, y) / 8;
  const uint8_t byte_offset = get_pixel_buffer_position_internal_(x, y) - (byte_location * 8);

  uint8_t color_byte = this->buffer_[byte_location];
  color_byte ^= (-(color.r + color.b + color.g == 0 ? 0 : 1) ^ color_byte) & (1UL << byte_offset);
  this->buffer_[byte_location] = color_byte;
}

uint8_t Bufferex1bit2color::get_color_bit_(int x, int y) {
  const uint32_t byte_location = get_pixel_buffer_position_internal_(x, y) / 8;
  const uint8_t byte_offset = get_pixel_buffer_position_internal_(x, y) - (byte_location * 8);

  auto color_byte = this->buffer_[byte_location];
  auto color_bit = (color_byte >> byte_offset) & 1U;
  return color_bit;
}

uint8_t Bufferex1bit2color::get_color_bit_(uint32_t pos) {
  const uint32_t byte_location = pos / 8;
  const uint8_t byte_offset = pos - (byte_location * 8);

  auto color_byte = this->buffer_[byte_location];
  auto color_bit = (color_byte >> byte_offset) & 1U;
  return color_bit;
}

uint16_t Bufferex1bit2color::get_pixel_to_565(int x, int y) {
  uint8_t color_bit = this->get_color_bit_(x, y);

  if (color_bit == 0)
    return ColorUtil::color_to_565(this->color_off_);

  return ColorUtil::color_to_565(this->color_on_);
}

uint16_t Bufferex1bit2color::get_pixel_to_565(uint32_t pos) {
  uint8_t color_bit = this->get_color_bit_(pos);

  if (color_bit == 0)
    return ColorUtil::color_to_565(this->color_off_);

  return ColorUtil::color_to_565(this->color_on_);
}

uint32_t Bufferex1bit2color::get_pixel_to_666(int x, int y) {
  uint8_t color_bit = this->get_color_bit_(x, y);

  if (color_bit == 0)
    return ColorUtil::color_to_666(this->color_off_, this->driver_right_bit_aligned_);

  return ColorUtil::color_to_666(this->color_on_, this->driver_right_bit_aligned_);
}

uint32_t Bufferex1bit2color::get_pixel_to_666(uint32_t pos) {
  uint8_t color_bit = this->get_color_bit_(pos);

  if (color_bit == 0)
    return ColorUtil::color_to_666(this->color_off_, this->driver_right_bit_aligned_);

  return ColorUtil::color_to_666(this->color_on_, this->driver_right_bit_aligned_);
}

size_t Bufferex1bit2color::get_buffer_length() {
  auto screensize = size_t(this->width_) * size_t(this->height_);
  auto buffsize = (screensize % 8) ? screensize / 8 + 1 : screensize / 8;
  return buffsize;
}

size_t Bufferex1bit2color::get_buffer_size() { return this->get_buffer_length(); }

}  // namespace display
}  // namespace esphome
