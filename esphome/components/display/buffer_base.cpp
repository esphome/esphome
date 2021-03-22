#include "buffer_base.h"

namespace esphome {
namespace display {
static const char *TAG = "buffer_base";

size_t BufferexBase::get_buffer_length() { return size_t(this->width_) * size_t(this->height_); }

void HOT BufferexBase::display_end() {
  // invalidate watermarks
  this->previous_info = this->current_info;

  this->current_info.x_low = this->width_;
  this->current_info.y_low = this->height_;
  this->current_info.x_high = 0;
  this->current_info.y_high = 0;
  this->pixel_count_ = 0;
}

uint16_t HOT BufferexBase::get_partial_update_x() {
  return this->get_partial_update_x_high() - this->get_partial_update_x_low() + 1;
}

uint16_t HOT BufferexBase::get_partial_update_y() {
  return this->get_partial_update_y_high() - this->get_partial_update_y_low() + 1;
}

uint16_t HOT BufferexBase::get_partial_update_x_low() {
  return this->current_info.x_low < this->previous_info.x_low ? this->current_info.x_low : this->previous_info.x_low;
}

uint16_t HOT BufferexBase::get_partial_update_x_high() {
  return this->current_info.x_high > this->previous_info.x_high ? this->current_info.x_high
                                                                : this->previous_info.x_high;
}

uint16_t HOT BufferexBase::get_partial_update_y_low() {
  return this->current_info.y_low < this->previous_info.y_low ? this->current_info.y_low : this->previous_info.y_low;
}

uint16_t HOT BufferexBase::get_partial_update_y_high() {
  return this->current_info.y_high > this->previous_info.y_high ? this->current_info.y_high
                                                                : this->previous_info.y_high;
}

void HOT BufferexBase::set_pixel(int x, int y, Color color) {
  if (x >= this->width_ || x < 0 || y >= this->height_ || y < 0) {
    ESP_LOGD(TAG, "set_pixel out of bounds");
    return;
  }

  bool result = this->set_buffer(x, y, color);
  if (!result) {
    return;
  }

  this->current_info.x_low = (x < this->current_info.x_low) ? x : this->current_info.x_low;
  this->current_info.y_low = (y < this->current_info.y_low) ? y : this->current_info.y_low;
  this->current_info.x_high = (x > this->current_info.x_high) ? x : this->current_info.x_high;
  this->current_info.y_high = (y > this->current_info.y_high) ? y : this->current_info.y_high;

  this->pixel_count_++;
}

}  // namespace display
}  // namespace esphome
