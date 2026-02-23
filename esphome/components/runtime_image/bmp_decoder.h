#pragma once

#include "esphome/core/defines.h"
#ifdef USE_RUNTIME_IMAGE_BMP

#include "image_decoder.h"
#include "runtime_image.h"

namespace esphome::runtime_image {

/**
 * @brief Image decoder specialization for BMP images.
 */
class BmpDecoder : public ImageDecoder {
 public:
  /**
   * @brief Construct a new BMP Decoder object.
   *
   * @param image The RuntimeImage to decode the stream into.
   */
  BmpDecoder(RuntimeImage *image) : ImageDecoder(image) {}

  int HOT decode(uint8_t *buffer, size_t size) override;

  bool is_finished() const override {
    // BMP is finished when we've decoded all pixel data
    return this->paint_index_ >= static_cast<size_t>(this->width_ * this->height_);
  }

 protected:
  size_t current_index_{0};
  size_t paint_index_{0};
  ssize_t width_{0};
  ssize_t height_{0};
  uint16_t bits_per_pixel_{0};
  uint32_t compression_method_{0};
  uint32_t image_data_size_{0};
  uint32_t color_table_entries_{0};
  size_t width_bytes_{0};
  size_t data_offset_{0};
  uint8_t padding_bytes_{0};
};

}  // namespace esphome::runtime_image

#endif  // USE_RUNTIME_IMAGE_BMP
