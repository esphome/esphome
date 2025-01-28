#include "image_decoder.h"
#include "online_image.h"

#include "esphome/core/log.h"

namespace esphome {
namespace online_image {

static const char *const TAG = "online_image.decoder";

bool ImageDecoder::set_size(int width, int height) {
  bool resized = this->image_->resize_(width, height);
  this->x_scale_ = static_cast<double>(this->image_->buffer_width_) / width;
  this->y_scale_ = static_cast<double>(this->image_->buffer_height_) / height;
  return resized;
}

void ImageDecoder::draw(int x, int y, int w, int h, const Color &color) {
  auto width = std::min(this->image_->buffer_width_, static_cast<int>(std::ceil((x + w) * this->x_scale_)));
  auto height = std::min(this->image_->buffer_height_, static_cast<int>(std::ceil((y + h) * this->y_scale_)));
  for (int i = x * this->x_scale_; i < width; i++) {
    for (int j = y * this->y_scale_; j < height; j++) {
      this->image_->draw_pixel_(i, j, color);
    }
  }
}

uint8_t *DownloadBuffer::data(size_t offset) {
  if (offset > this->size_) {
    ESP_LOGE(TAG, "Tried to access beyond download buffer bounds!!!");
    return this->buffer_;
  }
  return this->buffer_ + offset;
}

size_t DownloadBuffer::read(size_t len) {
  this->unread_ -= len;
  if (this->unread_ > 0) {
    memmove(this->data(), this->data(len), this->unread_);
  }
  return this->unread_;
}

size_t DownloadBuffer::resize(size_t size) {
  if (this->size_ == size) {
    return size;
  }
  this->allocator_.deallocate(this->buffer_, this->size_);
  this->size_ = size;
  this->buffer_ = this->allocator_.allocate(size);
  this->reset();
  if (this->buffer_) {
    return size;
  } else {
    return 0;
  }
}

}  // namespace online_image
}  // namespace esphome
