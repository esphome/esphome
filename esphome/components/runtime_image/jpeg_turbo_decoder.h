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
 * images.
 */
class JpegTurboDecoder : public ImageDecoder {
 public:
  /**
   * @brief Construct a new JPEG Turbo Decoder object.
   *
   * @param image The RuntimeImage to decode the stream into.
   */
  JpegTurboDecoder(RuntimeImage *image) : ImageDecoder(image, JPEG) {}

  int HOT decode(uint8_t *buffer, size_t size) override;
  // With an unknown size, decode() only succeeds once the whole image has arrived.
  bool is_finished() const override {
    return this->expected_size_ == 0 ? this->decoded_bytes_ > 0 : ImageDecoder::is_finished();
  }
};

}  // namespace esphome::runtime_image

#endif  // USE_RUNTIME_IMAGE_JPEG_TURBO
