#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "image_decoder.h"
#include "runtime_image.h"
#ifdef USE_RUNTIME_IMAGE_PNG
#include <pngle.h>

namespace esphome::runtime_image {

/**
 * @brief Image decoder specialization for PNG images.
 */
class PngDecoder : public ImageDecoder {
 public:
  /**
   * @brief Construct a new PNG Decoder object.
   *
   * @param image The RuntimeImage to decode the stream into.
   */
  PngDecoder(RuntimeImage *image);
  ~PngDecoder() override;

  int prepare(size_t expected_size) override;
  int HOT decode(uint8_t *buffer, size_t size) override;

  void increment_pixels_decoded(uint32_t count) { this->pixels_decoded_ += count; }
  uint32_t get_pixels_decoded() const { return this->pixels_decoded_; }

 protected:
  pngle_t *pngle_{nullptr};
  uint32_t pixels_decoded_{0};
};

}  // namespace esphome::runtime_image

#endif  // USE_RUNTIME_IMAGE_PNG
