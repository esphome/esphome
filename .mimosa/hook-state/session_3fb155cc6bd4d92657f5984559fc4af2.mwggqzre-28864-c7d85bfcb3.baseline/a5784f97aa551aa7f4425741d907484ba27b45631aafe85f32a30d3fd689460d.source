#include "image_decoder.h"
#include "runtime_image.h"
#include "esphome/components/display/display_color_utils.h"
#include "esphome/core/log.h"
#include <algorithm>

namespace esphome::runtime_image {

ESPHOME_LOG_TAG(TAG, "image_decoder");

bool ImageDecoder::set_size(int width, int height) {
  if (!this->resize_image_(width, height))
    return false;
  this->prepare_draw_(width, height);
  return true;
}

bool ImageDecoder::resize_image_(int width, int height) {
  this->size_valid_ = this->image_->resize(width, height) > 0;
  return this->size_valid_;
}

void ImageDecoder::prepare_draw_(int width, int height) {
  this->draw_width_ = width;
  this->draw_height_ = height;
  this->direct_rgb565_ = this->image_->get_type() == image::IMAGE_TYPE_RGB565 && !this->image_->has_transparency();
  if (!this->direct_rgb565_)
    return;
  if ((this->x_boundaries_.capacity() < static_cast<size_t>(width + 1) && !this->x_boundaries_.try_init(width + 1)) ||
      (this->y_boundaries_.capacity() < static_cast<size_t>(height + 1) && !this->y_boundaries_.try_init(height + 1))) {
    ESP_LOGW(TAG, "No memory to draw straight into the buffer; drawing pixel by pixel");
    this->direct_rgb565_ = false;
    // Free the table that did fit, as nothing uses it
    this->x_boundaries_.release();
    this->y_boundaries_.release();
    return;
  }
  this->x_boundaries_.clear();
  this->y_boundaries_.clear();
  const int buffer_width = this->image_->get_buffer_width();
  const int buffer_height = this->image_->get_buffer_height();
  // The same boundaries as draw() uses, so both paths paint the same pixels
  for (int x = 0; x <= width; x++) {
    this->x_boundaries_.push_back(static_cast<uint16_t>(scale_boundary(x, width, buffer_width)));
  }
  for (int y = 0; y <= height; y++) {
    this->y_boundaries_.push_back(static_cast<uint16_t>(scale_boundary(y, height, buffer_height)));
  }
}

void ImageDecoder::draw_rgb565_(int x, int y, int w, int h, uint16_t rgb565) {
  const int x_end = std::min(x + w, this->draw_width_);
  const int y_end = std::min(y + h, this->draw_height_);
  x = std::max(x, 0);
  y = std::max(y, 0);
  if (x >= x_end || y >= y_end)
    return;
  // A block of source pixels lands on one block of the buffer
  const uint16_t x1 = this->x_boundaries_[x];
  const uint16_t x2 = this->x_boundaries_[x_end];
  const uint16_t y1 = this->y_boundaries_[y];
  const uint16_t y2 = this->y_boundaries_[y_end];
  const bool big_endian = this->image_is_big_endian_();
  const uint8_t first = big_endian ? rgb565 >> 8 : rgb565 & 0xFF;
  const uint8_t second = big_endian ? rgb565 & 0xFF : rgb565 >> 8;
  uint8_t *const buffer = this->image_buffer_();
  const uint32_t buffer_width = this->image_->get_buffer_width();
  for (uint32_t row = y1; row < y2; row++) {
    uint8_t *destination = buffer + (row * buffer_width + x1) * 2;
    for (uint32_t col = x1; col < x2; col++) {
      destination[0] = first;
      destination[1] = second;
      destination += 2;
    }
  }
}

void ImageDecoder::draw(int x, int y, int w, int h, const Color &color) {
  if (!this->size_valid_) {
    return;
  }
  if (this->direct_rgb565_) {
    this->draw_rgb565_(x, y, w, h, display::ColorUtil::color_to_565(color));
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
