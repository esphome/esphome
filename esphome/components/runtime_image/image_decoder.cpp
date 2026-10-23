#include "image_decoder.h"
#include "runtime_image.h"
#include "esphome/core/log.h"
#include <algorithm>

namespace esphome::runtime_image {

ESPHOME_LOG_TAG(TAG, "image_decoder");

bool ImageDecoder::set_size(int width, int height) {
  bool success = this->image_->resize(width, height) > 0;
  this->size_valid_ = success;
  this->draw_width_ = width;
  this->draw_height_ = height;
  return success;
}

void ImageDecoder::draw(int x, int y, int w, int h, const Color &color) {
  if (!this->size_valid_) {
    return;
  }
  // When shrinking, each target pixel is painted once, by the first source pixel that reaches it, rather than by every
  // pixel that lands on it. Integer maths: the S3 has no double FPU.
  const int buffer_width = this->image_->get_buffer_width();
  const int buffer_height = this->image_->get_buffer_height();
  int x1 = x;
  int y1 = y;
  int x2 = x + w;
  int y2 = y + h;
  if (buffer_width != this->draw_width_ || buffer_height != this->draw_height_) {
    x1 = scale_boundary(x, this->draw_width_, buffer_width);
    y1 = scale_boundary(y, this->draw_height_, buffer_height);
    x2 = scale_boundary(x + w, this->draw_width_, buffer_width);
    y2 = scale_boundary(y + h, this->draw_height_, buffer_height);
  }
  x2 = std::min(buffer_width, x2);
  y2 = std::min(buffer_height, y2);
  for (int i = x1; i < x2; i++) {
    for (int j = y1; j < y2; j++) {
      this->image_->draw_pixel(i, j, color);
    }
  }
}

uint8_t *ImageDecoder::image_buffer_() const { return this->image_->buffer_; }
bool ImageDecoder::image_is_big_endian_() const { return this->image_->big_endian_; }

}  // namespace esphome::runtime_image
