#include "display_buffer.h"

#include <utility>

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome {
namespace display {

static const char *const TAG = "display";

void DisplayBuffer::init_internal_(uint32_t buffer_length) {
  ExternalRAMAllocator<uint8_t> allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
  this->buffer_ = allocator.allocate(buffer_length);
  if (this->buffer_ == nullptr) {
    ESP_LOGE(TAG, "Could not allocate buffer for display!");
    return;
  }
  this->clear();
}

int DisplayBuffer::get_width() {
  switch (this->rotation_) {
    case DISPLAY_ROTATION_90_DEGREES:
    case DISPLAY_ROTATION_270_DEGREES:
      return this->get_height_internal();
    case DISPLAY_ROTATION_0_DEGREES:
    case DISPLAY_ROTATION_180_DEGREES:
    default:
      return this->get_width_internal();
  }
}

int DisplayBuffer::get_height() {
  switch (this->rotation_) {
    case DISPLAY_ROTATION_0_DEGREES:
    case DISPLAY_ROTATION_180_DEGREES:
      return this->get_height_internal();
    case DISPLAY_ROTATION_90_DEGREES:
    case DISPLAY_ROTATION_270_DEGREES:
    default:
      return this->get_width_internal();
  }
}

void HOT DisplayBuffer::draw_pixel_at(int x, int y, Color color) {
  if (!this->get_clipping().inside(x, y))
    return;  // NOLINT

  switch (this->rotation_) {
    case DISPLAY_ROTATION_0_DEGREES:
      break;
    case DISPLAY_ROTATION_90_DEGREES:
      std::swap(x, y);
      x = this->get_width_internal() - x - 1;
      break;
    case DISPLAY_ROTATION_180_DEGREES:
      x = this->get_width_internal() - x - 1;
      y = this->get_height_internal() - y - 1;
      break;
    case DISPLAY_ROTATION_270_DEGREES:
      std::swap(x, y);
      y = this->get_height_internal() - y - 1;
      break;
  }
  this->draw_absolute_pixel_internal(x, y, color);
  App.feed_wdt();
}

void DisplayBuffer::draw_vertical_pixel_group(int x, int y_row, uint8_t colors, uint8_t transparency,
                                              bool reverse_bit_order) {
  Rect clipping = this->get_clipping();
  if (clipping.is_set()) {
    if (x < clipping.x || x > clipping.x2() || y_row * 8 + 7 < clipping.y || y_row * 8 > clipping.y2()) {
      return;
    }
    int16_t clip_top = clipping.y - y_row * 8;
    int16_t clip_bottom = (y_row * 8 + 7) - clipping.y2();
    if (clip_top > 0) {
      transparency &= ~(0xff << (8 - clip_top));
    }
    if (clip_bottom > 0) {
      transparency &= ~(0xff >> (8 - clip_bottom));
    }
  }
  int x_column, y;
  switch (this->rotation_) {
    case DISPLAY_ROTATION_0_DEGREES:
      this->draw_vertical_pixel_group_internal(x, y_row, colors, transparency, reverse_bit_order);
      break;
    case DISPLAY_ROTATION_90_DEGREES:
      x_column = y_row;
      y = x;
      x_column = this->get_width_internal() / 8 - x_column - 1;
      this->draw_horizontal_pixel_group_internal(x_column, y, colors, transparency, reverse_bit_order);
      break;
    case DISPLAY_ROTATION_180_DEGREES:
      x = this->get_width_internal() - x - 1;
      y_row = this->get_height_internal() / 8 - y_row - 1;
      this->draw_vertical_pixel_group_internal(x, y_row, colors, transparency, !reverse_bit_order);
      break;
    case DISPLAY_ROTATION_270_DEGREES:
      x_column = y_row;
      y = x;
      y = this->get_height_internal() - y - 1;
      this->draw_horizontal_pixel_group_internal(x_column, y, colors, transparency, !reverse_bit_order);
      break;
  }
  App.feed_wdt();
}

void DisplayBuffer::draw_horizontal_pixel_group(int x_column, int y, uint8_t colors, uint8_t transparency,
                                                bool reverse_bit_order) {
  Rect clipping = this->get_clipping();
  if (clipping.is_set()) {
    if (y < clipping.y || y > clipping.y2() || x_column * 8 + 7 < clipping.x || x_column * 8 > clipping.x2()) {
      return;
    }
    int16_t clip_left = clipping.x - x_column * 8;
    int16_t clip_right = (x_column * 8 + 7) - clipping.x2();
    if (clip_left > 0) {
      transparency &= ~(0xff << (8 - clip_left));
    }
    if (clip_right > 0) {
      transparency &= ~(0xff >> (8 - clip_right));
    }
  }
  int x, y_row;
  switch (this->rotation_) {
    case DISPLAY_ROTATION_0_DEGREES:
      this->draw_horizontal_pixel_group_internal(x_column, y, colors, transparency, reverse_bit_order);
      break;
    case DISPLAY_ROTATION_90_DEGREES:
      x = y;
      y_row = x_column;
      x = this->get_width_internal() - x - 1;
      this->draw_vertical_pixel_group_internal(x, y_row, colors, transparency, reverse_bit_order);
      break;
    case DISPLAY_ROTATION_180_DEGREES:
      x_column = this->get_width_internal() / 8 - x_column - 1;
      y = this->get_height_internal() - y - 1;
      this->draw_horizontal_pixel_group_internal(x_column, y, colors, transparency, !reverse_bit_order);
      break;
    case DISPLAY_ROTATION_270_DEGREES:
      x = y;
      y_row = x_column;
      y_row = this->get_height_internal() / 8 - y_row - 1;
      this->draw_vertical_pixel_group_internal(x, y_row, colors, transparency, !reverse_bit_order);
      break;
  }
  App.feed_wdt();
}

PixelGroupMode DisplayBuffer::get_pixel_group_mode() {
  switch (this->rotation_) {
    case DISPLAY_ROTATION_90_DEGREES:
    case DISPLAY_ROTATION_270_DEGREES:
      return (PixelGroupMode) ((uint8_t) this->get_pixel_group_mode_internal() ^ 0x03);
    case DISPLAY_ROTATION_0_DEGREES:
    case DISPLAY_ROTATION_180_DEGREES:
    default:
      return this->get_pixel_group_mode_internal();
  }
}

uint8_t HOT DisplayBuffer::reverse_bit_order(uint8_t data) {
  if (data == 0xFF)
    return 0xFF;
  data = (data & 0xF0) >> 4 | (data & 0x0F) << 4;
  data = (data & 0xCC) >> 2 | (data & 0x33) << 2;
  data = (data & 0xAA) >> 1 | (data & 0x55) << 1;
  return data;
}

}  // namespace display
}  // namespace esphome
