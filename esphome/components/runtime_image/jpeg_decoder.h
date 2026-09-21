#pragma once

#include "image_decoder.h"
#include "runtime_image.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
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

  bool uses_direct_rgb565() const { return this->direct_rgb565_; }
  void draw_rgb565(int x, int y, int w, int h, const uint8_t *pixels);

 protected:
  bool prepare_rgb565_scaling_(int width, int height);

  JPEGDEC jpeg_{};
  FixedVector<uint16_t> x_boundaries_{};
  FixedVector<uint16_t> y_boundaries_{};
  int source_width_{0};
  int source_height_{0};
  bool direct_rgb565_{false};
};

}  // namespace esphome::runtime_image

#endif  // USE_RUNTIME_IMAGE_JPEG
