#pragma once

#include "image_decoder.h"
#include "runtime_image.h"
#include "esphome/core/defines.h"
// When libjpeg-turbo is selected it replaces JPEGDEC for the whole build,
// and the JPEGDEC library (and its header) is not available.
#if defined(USE_RUNTIME_IMAGE_JPEG) && !defined(USE_RUNTIME_IMAGE_JPEG_TURBO)
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

#endif  // USE_RUNTIME_IMAGE_JPEG && !USE_RUNTIME_IMAGE_JPEG_TURBO
