#ifdef USE_ARDUINO

#include "image_decoder.h"
#include "online_image.h"

namespace esphome {
namespace online_image {

static const char *const TAG = "online_image.decoder";

void ImageDecoder::set_size(int width, int height) {
  image_->resize_(width, height);
  x_scale_ = static_cast<double>(image_->get_width()) / width;
  y_scale_ = static_cast<double>(image_->get_height()) / height;
}

void ImageDecoder::draw(int x, int y, int w, int h, const Color &color) {
  auto width = std::min(image_->get_width(), static_cast<int>(std::ceil((x + w) * x_scale_)));
  auto height = std::min(image_->get_height(), static_cast<int>(std::ceil((y + h) * y_scale_)));
  for (int i = x * x_scale_; i < width; i++) {
    for (int j = y * y_scale_; j < height; j++) {
      image_->draw_pixel_(i, j, color);
    }
  }
}

}  // namespace online_image
}  // namespace esphome

#endif  // USE_ARDUINO
