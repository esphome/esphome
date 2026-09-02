#pragma once

#include "image_decoder.h"
#include "runtime_image.h"
#include "esphome/core/defines.h"
#ifdef USE_RUNTIME_IMAGE_JPEG_TURBO

namespace esphome::runtime_image {

/**
 * @brief Image decoder specialization for JPEG images based on libjpeg-turbo.
 *
 * Unlike the JPEGDEC based decoder, this one also supports progressive JPEG
 * images. Note that progressive decoding buffers the coefficients of the whole
 * image in memory, so large progressive images generally require PSRAM.
 */
class JpegTurboDecoder : public ImageDecoder {
 public:
  /**
   * @brief Construct a new JPEG Turbo Decoder object.
   *
   * @param image The RuntimeImage to decode the stream into.
   */
  JpegTurboDecoder(RuntimeImage *image) : ImageDecoder(image, JPEG) {}

  int prepare(size_t expected_size) override;
  int HOT decode(uint8_t *buffer, size_t size) override;
};

}  // namespace esphome::runtime_image

#endif  // USE_RUNTIME_IMAGE_JPEG_TURBO
