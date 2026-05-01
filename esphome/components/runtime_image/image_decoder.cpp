#include "image_decoder.h"
#include "runtime_image.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cmath>

namespace esphome::runtime_image {

static const char *const TAG = "image_decoder";

bool ImageDecoder::set_size(int width, int height) {
  bool success = this->image_->resize(width, height) > 0;
  this->x_scale_ = static_cast<double>(this->image_->get_buffer_width()) / width;
  this->y_scale_ = static_cast<double>(this->image_->get_buffer_height()) / height;
  return success;
}

void ImageDecoder::draw(int x, int y, int w, int h, const Color &color) {
  auto width = std::min(this->image_->get_buffer_width(), static_cast<int>(std::ceil((x + w) * this->x_scale_)));
  auto height = std::min(this->image_->get_buffer_height(), static_cast<int>(std::ceil((y + h) * this->y_scale_)));
  for (int i = x * this->x_scale_; i < width; i++) {
    for (int j = y * this->y_scale_; j < height; j++) {
      this->image_->draw_pixel(i, j, color);
    }
  }
}

}  // namespace esphome::runtime_image
