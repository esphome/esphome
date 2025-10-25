#pragma once

#include "image_decoder.h"
#include "esphome/core/defines.h"
#ifdef USE_ONLINE_IMAGE_JPEG_SUPPORT
#include <JPEGDEC.h>

namespace esphome {
namespace online_image {

/**
 * @brief Image decoder specialization for JPEG images.
 */
class JpegDecoder : public ImageDecoder {
 public:
  /**
   * @brief Construct a new JPEG Decoder object.
   *
   * @param display The image to decode the stream into.
   */
  JpegDecoder(OnlineImage *image) : ImageDecoder(image) {}
  ~JpegDecoder() override {}

  int prepare(size_t download_size) override;
  int HOT decode(uint8_t *buffer, size_t size) override;

 protected:
  JPEGDEC jpeg_{};
};

}  // namespace online_image
}  // namespace esphome

#endif  // USE_ONLINE_IMAGE_JPEG_SUPPORT
