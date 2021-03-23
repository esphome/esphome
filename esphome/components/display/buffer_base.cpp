#include "buffer_base.h"

namespace esphome {
namespace display {
static const char *TAG = "buffer_base";

size_t BufferexBase::get_buffer_length() { return size_t(this->width_) * size_t(this->height_); }

#ifdef NO_PARTIAL
void HOT BufferexBase::display_end() {}
#else
void HOT BufferexBase::display_end() {
  // invalidate watermarks
  this->previous_info = this->current_info;

  this->current_info.x_low = this->width_;
  this->current_info.y_low = this->height_;
  this->current_info.x_high = 0;
  this->current_info.y_high = 0;
  this->pixel_count_ = 0;
}

void HOT BufferexBase::reset_partials() {
  this->current_info.x_low = 0;
  this->current_info.y_low = 0;
  this->current_info.x_high = this->width_ - 1;
  this->current_info.y_high = this->height_ - 1;
  this->previous_info = this->current_info;
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
#endif

void HOT BufferexBase::set_pixel(int x, int y, Color color) {
  if (x >= this->width_ || x < 0 || y >= this->height_ || y < 0) {
    ESP_LOGVV(TAG, "set_pixel out of bounds %d/%d %d/%d", x, this->width_, y, this->height_);
    return;
  }

  bool result = this->set_buffer(x, y, color);
#ifndef NO_PARTIAL
  if (!result) {
    return;
  }

  this->current_info.x_low = (x < this->current_info.x_low) ? x : this->current_info.x_low;
  this->current_info.y_low = (y < this->current_info.y_low) ? y : this->current_info.y_low;
  this->current_info.x_high = (x > this->current_info.x_high) ? x : this->current_info.x_high;
  this->current_info.y_high = (y > this->current_info.y_high) ? y : this->current_info.y_high;

  this->pixel_count_++;
#endif
}

}  // namespace display
}  // namespace esphome
