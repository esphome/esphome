#include "buffer_indexed8.h"

namespace esphome {
namespace display {

#ifdef USE_BUFFER_INDEXED8
static const char *TAG = "buffer_indexed8";

bool BufferexIndexed8::init_buffer(int width, int height) {
  this->width_ = width;
  this->height_ = height;

  this->buffer_ = new_buffer<uint8_t>(this->get_buffer_length());
  if (this->buffer_ == nullptr) {
    return false;
  }

  memset(this->buffer_, 0x00, this->get_buffer_size());
  return true;
}

uint8_t HOT BufferexIndexed8::get_index_from_color_(Color color) {
  for (int i = 0; i < this->colors_.size(); i++) {
    if (this->colors_[i].raw_32 == color.raw_32) {
      return i;
    }
  }
  return this->default_index_value_;
}

uint8_t HOT BufferexIndexed8::get_value_from_color_index_(uint8_t index) {
  for (int i = 0; i < this->indexed_colors_.size(); i++) {
    if (this->indexed_colors_[i].raw_32 == this->colors_[index].raw_32) {
      return i;
    }
  }
  return this->default_index_value_;
}

void HOT BufferexIndexed8::fill_buffer(Color color) {
  display::BufferexBase::fill_buffer(color);
  int count = 0;
  ESP_LOGD(TAG, "fill_buffer %d %d/%d", color.g, this->width_, this->height_);
  for (uint16_t h = 0; h < this->height_; h++) {
    for (uint16_t w = 0; w < this->width_; w++) {
      if (this->set_buffer(w, h, color))
        ++count;
    }
  }
  ESP_LOGD(TAG, "fill_buffer done, count %d", count);
}

bool HOT BufferexIndexed8::set_buffer(int x, int y, Color color) {
  bool result = false;

  uint32_t pos = this->get_pixel_buffer_position_(x, y);

  bool debug = false;

  uint8_t index = this->get_index_from_color_(color);

  if (debug && y == 0 && (color.g != 0 || index != 0))
    ESP_LOGD(TAG, "set_buffer raw %d %d %d", color.raw_32, index, pos);

  const uint32_t pixel_bit_start = pos * this->pixel_storage_size_;
  const uint32_t pixel_bit_end = pixel_bit_start + this->pixel_storage_size_;

  const uint32_t byte_location_start = pixel_bit_start / 8;
  const uint32_t byte_location_end = pixel_bit_end / 8;

  const uint8_t byte_offset_start = pixel_bit_start % 8;

  uint8_t index_byte_start = this->buffer_[byte_location_start];
  uint8_t mask = ((1 << this->pixel_storage_size_) - 1) << byte_offset_start;

  index_byte_start = (index_byte_start & ~mask) | ((index << byte_offset_start) & mask);

  // ESP_LOGD(TAG, "set_buffer byte_location_start %d", byte_location_start);
  if (this->buffer_[byte_location_start] != index_byte_start) {
    this->buffer_[byte_location_start] = index_byte_start;
    result = true;
  }

  if (byte_location_start == byte_location_end) {
    // Pixel starts and ends in the same byte, so we're done.
    return result;
  }

  const uint8_t byte_offset_end = pixel_bit_end % 8;

  uint8_t index_byte_end = this->buffer_[byte_location_end];
  mask = (((uint8_t) 1 << this->pixel_storage_size_) - 1) >> (this->pixel_storage_size_ - byte_offset_end);

  index_byte_end = (index_byte_end & ~mask) | ((index >> (this->pixel_storage_size_ - byte_offset_end)) & mask);
  if (this->buffer_[byte_location_end] != index_byte_end) {
    this->buffer_[byte_location_end] = index_byte_end;
    result = true;
  }
  return false;
}

uint8_t HOT BufferexIndexed8::get_index_value_(int x, int y) { return this->get_index_value_((x + y * this->width_)); }

uint8_t HOT BufferexIndexed8::get_index_value_(uint32_t pos) {
  const uint32_t pixel_bit_start = pos * this->pixel_storage_size_;
  const uint32_t pixel_bit_end = pixel_bit_start + this->pixel_storage_size_;

  const uint32_t byte_location_start = pixel_bit_start / 8;
  const uint32_t byte_location_end = pixel_bit_end / 8;

  //  ESP_LOGE(TAG, "get_index_value_ %d exceeds buffer length %d", pos, this->get_buffer_length());
  uint8_t index_byte_start = this->buffer_[byte_location_start];
  const uint8_t byte_offset_start = pixel_bit_start % 8;

  uint8_t mask = (1 << this->pixel_storage_size_) - 1;

  index_byte_start = (index_byte_start >> byte_offset_start);

  if (byte_location_start == byte_location_end) {
    // Pixel starts and ends in the same byte, so we're done.
    return index_byte_start & mask;
  }

  const uint8_t byte_offset_end = pixel_bit_end % 8;

  uint8_t end_mask = mask >> (this->pixel_storage_size_ - byte_offset_end);

  uint8_t index_byte_end = this->buffer_[byte_location_end];

  index_byte_end = (index_byte_end & end_mask) << (this->pixel_storage_size_ - byte_offset_end);

  index_byte_end = index_byte_end | index_byte_start;

  return index_byte_end & mask;
}

uint8_t HOT BufferexIndexed8::get_pixel_value(uint32_t pos) {
  uint8_t value = this->get_index_value_(pos);

  if (value > this->index_size_)
    value = 0;

  return this->get_value_from_color_index_(value);
}

uint16_t HOT BufferexIndexed8::get_pixel_to_565(uint32_t pos) {
  uint8_t value = this->get_index_value_(pos);

  if (value > this->index_size_)
    value = 0;

  return ColorUtil::color_to_565(this->colors_[value]);
}

uint32_t HOT BufferexIndexed8::get_pixel_to_666(uint32_t pos) {
  uint8_t value = this->get_index_value_(pos);

  if (value > this->index_size_)
    value = 0;

  return ColorUtil::color_to_666(this->colors_[value], this->driver_right_bit_aligned_);
}

size_t HOT BufferexIndexed8::get_buffer_length() {  // How many unint8_t bytes does the buffer need
  if (this->get_buffer_length_ != 0)
    return this->get_buffer_length_;

  auto total_pixels = size_t(this->width_) * size_t(this->height_);

  if (this->index_size_ <= 1) {
    this->pixel_storage_size_ = 1;
  } else {
    this->pixel_storage_size_ = std::ceil(std::log(this->index_size_) / std::log(2));
  }

  auto screensize = total_pixels * pixel_storage_size_;

  auto bufflength = (screensize % 8) ? screensize / 8 + 1 : screensize / 8;

  ESP_LOGD(TAG, "Pixel index size: %hhu", this->index_size_);
  ESP_LOGD(TAG, "Total Pixels: %zu", total_pixels);
  ESP_LOGD(TAG, "Pixel Storage Size: %d", this->pixel_storage_size_);
  ESP_LOGD(TAG, "Buffer length %zu", bufflength);

  this->get_buffer_length_ = bufflength;

  return bufflength;
}

size_t HOT BufferexIndexed8::get_buffer_size() { return this->get_buffer_length(); }
#endif
}  // namespace display
}  // namespace esphome
