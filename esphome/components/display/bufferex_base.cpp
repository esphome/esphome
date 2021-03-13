#include "bufferex_base.h"

namespace esphome {
namespace display {
static const char *TAG = "bufferex_base";

size_t BufferexBase::get_buffer_length() { return size_t(this->width_) * size_t(this->height_); }

void HOT BufferexBase::display() {
  // invalidate watermarks
  this->x_low_ = this->width_;
  this->y_low_ = this->height_;
  this->x_high_ = 0;
  this->y_high_ = 0;
}

void HOT BufferexBase::set_pixel(int x, int y, Color color) {
  if (x >= this->width_ || x < 0 || y >= this->height_ || y < 0) {
    ESP_LOGD(TAG, "set_pixel out of bounds");
    return;
  }

  this->x_low_ = (x < this->x_low_) ? x : this->x_low_;
  this->y_low_ = (y < this->y_low_) ? y : this->y_low_;
  this->x_high_ = (x > this->x_high_) ? x : this->x_high_;
  this->y_high_ = (y > this->y_high_) ? y : this->y_high_;

  this->set_buffer(x, y, color);
}

void BufferexBase::fill_buffer(Color color) {
  ESP_LOGD(TAG, "fill_buffer");
  this->x_low_ = 0;
  this->y_low_ = 0;
  this->x_high_ = this->width_ - 1;
  this->y_high_ = this->height_ - 1;
}

}  // namespace display
}  // namespace esphome
