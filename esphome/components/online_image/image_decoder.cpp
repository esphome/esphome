#ifdef USE_ARDUINO

#include "image_decoder.h"
#include "online_image.h"

namespace esphome {
namespace online_image {

static const char *const TAG = "online_image.decoder";

void ImageDecoder::set_size(int width, int height) {
  image_->resize_(width, height);
  x_scale_ = static_cast<double>(image_->buffer_width_) / width;
  y_scale_ = static_cast<double>(image_->buffer_height_) / height;
}

void ImageDecoder::draw(int x, int y, int w, int h, const Color &color) {
  auto width = std::min(image_->buffer_width_, static_cast<int>(std::ceil((x + w) * x_scale_)));
  auto height = std::min(image_->buffer_height_, static_cast<int>(std::ceil((y + h) * y_scale_)));
  for (int i = x * x_scale_; i < width; i++) {
    for (int j = y * y_scale_; j < height; j++) {
      image_->draw_pixel_(i, j, color);
    }
  }
}

uint8_t *DownloadBuffer::data(size_t offset) {
  if (offset > buffer_.size()) {
    ESP_LOGE(TAG, "Tried to access beyond download buffer bounds!!!");
    return buffer_.data();
  }
  return buffer_.data() + offset;
}

size_t DownloadBuffer::read(size_t len) {
  unread_ -= len;
  if (unread_ > 0) {
    memmove(data(), data(len), unread_);
  }
  return unread_;
}

}  // namespace online_image
}  // namespace esphome

#endif  // USE_ARDUINO
