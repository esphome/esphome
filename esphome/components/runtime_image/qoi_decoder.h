#pragma once

#include "esphome/core/defines.h"
#ifdef USE_RUNTIME_IMAGE_QOI

#include <memory>

#include "image_decoder.h"
#include "runtime_image.h"

namespace esphome::runtime_image {

/**
 * @brief Image decoder specialization for QOI images.
 */
class QoiDecoder : public ImageDecoder {
 public:
  /**
   * @brief Construct a new QOI decoder object.
   *
   * @param image The RuntimeImage to decode the stream into.
   */
  QoiDecoder(RuntimeImage *image) : ImageDecoder(image, QOI) {}

  void reset() override;
  int HOT decode(uint8_t *buffer, size_t size) override;

  bool is_finished() const override {
    if (this->bits_per_pixel_ == 0) {
      // header not yet received, so dimensions not yet determined
      return false;
    }
    // QOI is finished when we've decoded all pixel data
    return this->paint_index_ >= static_cast<size_t>(this->width_ * this->height_);
  }

 protected:
  std::unique_ptr<Color[]> color_table_;
  size_t current_index_{0};
  size_t paint_index_{0};
  size_t width_{0};
  size_t height_{0};
  Color last_pixel_{0, 0, 0, 255};  // QOI spec defines initial previous pixel as opaque black
  uint16_t bits_per_pixel_{0};
};

}  // namespace esphome::runtime_image

#endif  // USE_RUNTIME_IMAGE_QOI
