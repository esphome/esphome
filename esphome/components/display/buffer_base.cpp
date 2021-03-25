#include "buffer_base.h"

namespace esphome {
namespace display {
static const char *TAG = "buffer_base";

size_t BufferBase::get_buffer_length() { return size_t(this->width_) * size_t(this->height_); }

#ifdef NO_PARTIAL
void HOT BufferBase::display_end() {}
#else
void HOT BufferBase::display_end() {
  // invalidate watermarks
  ESP_LOGD(TAG, "display_end %d/%d %d/%d %d/%d", this->width_, this->height_, this->current_info.y_low,
           this->previous_info.y_low, this->current_info.y_high, this->previous_info.y_high);
  this->previous_info = this->current_info;

  this->current_info.x_low = this->width_;
  this->current_info.y_low = this->height_;
  this->current_info.x_high = 0;
  this->current_info.y_high = 0;
  this->pixel_count_ = 0;
}

void HOT BufferBase::reset_partials() {
  this->current_info.x_low = 0;
  this->current_info.y_low = 0;
  this->current_info.x_high = this->width_;
  this->current_info.y_high = this->height_;

  //  this->previous_info = this->current_info;

  this->previous_info.x_low = this->width_;
  this->previous_info.y_low = this->height_;
  this->previous_info.x_high = 0;
  this->previous_info.y_high = 0;

  this->pixel_count_ = 0;
}

uint16_t HOT BufferBase::get_partial_update_x() {
  ESP_LOGD(TAG, "get_partial_update_x %d/%d", this->get_partial_update_x_high(), this->get_partial_update_x_low());

  if (this->get_partial_update_x_high() - this->get_partial_update_x_low() < 1) {
    return this->width_;
  }

  return this->get_partial_update_x_high() - this->get_partial_update_x_low();
}

uint16_t HOT BufferBase::get_partial_update_y() {
  if (this->get_partial_update_y_high() - this->get_partial_update_y_low() < 1) {
    return this->height_;
  }
  ESP_LOGD(TAG, "get_partial_update_y %d/%d", this->get_partial_update_y_high(), this->get_partial_update_y_low());
  return this->get_partial_update_y_high() - this->get_partial_update_y_low();
}

uint16_t HOT BufferBase::get_partial_update_x_low() {
  return this->current_info.x_low < this->previous_info.x_low ? this->current_info.x_low : this->previous_info.x_low;
}

uint16_t HOT BufferBase::get_partial_update_x_high() {
  return this->current_info.x_high > this->previous_info.x_high ? this->current_info.x_high
                                                                : this->previous_info.x_high;
}

uint16_t HOT BufferBase::get_partial_update_y_low() {
  return this->current_info.y_low < this->previous_info.y_low ? this->current_info.y_low : this->previous_info.y_low;
}

uint16_t HOT BufferBase::get_partial_update_y_high() {
  return this->current_info.y_high > this->previous_info.y_high ? this->current_info.y_high
                                                                : this->previous_info.y_high;
}
#endif

void HOT BufferBase::set_pixel(int x, int y, Color color) {
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

void HOT BufferBase::set_pixel(int x, int y, uint8_t raw_value) {
  if (x >= this->width_ || x < 0 || y >= this->height_ || y < 0) {
    ESP_LOGVV(TAG, "set_pixel out of bounds %d/%d %d/%d", x, this->width_, y, this->height_);
    return;
  }

  bool result = this->set_buffer(x, y, raw_value);
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
