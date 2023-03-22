#ifdef USE_ARDUINO

#include "image_decoder.h"
#include "online_image.h"

namespace esphome {
namespace online_image {

static const char *const TAG = "online_image.decoder";

void ImageDecoder::set_size(uint32_t width, uint32_t height) {
    image_->resize(width, height);
    x_scale_ = (double)image_->get_width() / width;
    y_scale_ = (double)image_->get_height() / height;
}

void ImageDecoder::draw(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const Color &color) {
  auto width = std::min(image_->get_width(), static_cast<int>(std::ceil((x + w) * x_scale_)));
  auto height = std::min(image_->get_height(), static_cast<int>(std::ceil((y + h) * y_scale_)));
  for (uint32_t i = x * x_scale_; i < width; i++) {
    for (uint32_t j = y * y_scale_; j < height; j++) {
      image_->draw_pixel(i, j, color);
    }
  }
}

}  // namespace online_image
}  // namespace esphome

#endif  // USE_ARDUINO
