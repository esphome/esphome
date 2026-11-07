#pragma once

#include "image_decoder.h"
#include "runtime_image.h"
#include "esphome/core/defines.h"
#ifdef USE_RUNTIME_IMAGE_JPEG
#include <JPEGDEC.h>

namespace esphome::runtime_image {

/**
 * @brief Image decoder specialization for JPEG images.
 */
class JpegDecoder : public ImageDecoder {
 public:
  /**
   * @brief Construct a new JPEG Decoder object.
   *
   * @param image The RuntimeImage to decode the stream into.
   */
  JpegDecoder(RuntimeImage *image) : ImageDecoder(image, JPEG) {}
  ~JpegDecoder() override {}

  int HOT decode(uint8_t *buffer, size_t size) override;

 protected:
  JPEGDEC jpeg_{};
};

}  // namespace esphome::runtime_image

#endif  // USE_RUNTIME_IMAGE_JPEG
