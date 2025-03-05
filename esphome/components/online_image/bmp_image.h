#pragma once

#include "esphome/core/defines.h"
#ifdef USE_ONLINE_IMAGE_BMP_SUPPORT

#include "image_decoder.h"

namespace esphome {
namespace online_image {

/**
 * @brief Image decoder specialization for PNG images.
 */
class BmpDecoder : public ImageDecoder {
 public:
  /**
   * @brief Construct a new BMP Decoder object.
   *
   * @param display The image to decode the stream into.
   */
  BmpDecoder(OnlineImage *image) : ImageDecoder(image) {}

  int HOT decode(uint8_t *buffer, size_t size) override;

 protected:
  size_t current_index_{0};
  ssize_t width_{0};
  ssize_t height_{0};
  uint16_t bits_per_pixel_{0};
  uint32_t compression_method_{0};
  uint32_t image_data_size_{0};
  uint32_t color_table_entries_{0};
  size_t width_bytes_{0};
  size_t data_offset_{0};
};

}  // namespace online_image
}  // namespace esphome

#endif  // USE_ONLINE_IMAGE_BMP_SUPPORT
