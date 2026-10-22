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

int ImageDecoder::scaled_x_(int x) const {
  if (this->draw_width_ <= 0)
    return 0;
  const uint32_t buffer_width = this->image_->get_buffer_width();
  return static_cast<int>((static_cast<uint32_t>(x) * buffer_width + this->draw_width_ - 1) / this->draw_width_);
}

int ImageDecoder::scaled_y_(int y) const {
  if (this->draw_height_ <= 0)
    return 0;
  const uint32_t buffer_height = this->image_->get_buffer_height();
  return static_cast<int>((static_cast<uint32_t>(y) * buffer_height + this->draw_height_ - 1) / this->draw_height_);
}

void ImageDecoder::draw(int x, int y, int w, int h, const Color &color) {
  if (!this->size_valid_) {
    return;
  }
  // A source pixel paints the target pixels from ceil(start * scale) up to ceil(end * scale). The spans of neighbouring
  // pixels meet without overlapping, so when an image is shrunk each target pixel is painted once, by the first
  // source pixel that reaches it, rather than by every pixel that lands on it. Integer maths: the S3 has no double FPU.
  const int buffer_width = this->image_->get_buffer_width();
  const int buffer_height = this->image_->get_buffer_height();
  const int x1 = this->scaled_x_(x);
  const int y1 = this->scaled_y_(y);
  const int x2 = std::min(buffer_width, this->scaled_x_(x + w));
  const int y2 = std::min(buffer_height, this->scaled_y_(y + h));
  for (int i = x1; i < x2; i++) {
    for (int j = y1; j < y2; j++) {
      this->image_->draw_pixel(i, j, color);
    }
  }
}

uint8_t *ImageDecoder::image_buffer_() const { return this->image_->buffer_; }
bool ImageDecoder::image_is_big_endian_() const { return this->image_->big_endian_; }

}  // namespace esphome::runtime_image
