#pragma once

#include "image_decoder.h"
#include "esphome/core/defines.h"
#ifdef USE_ONLINE_IMAGE_PNG_SUPPORT
#include <pngle.h>

namespace esphome {
namespace online_image {

/**
 * @brief Image decoder specialization for PNG images.
 */
class PngDecoder : public ImageDecoder {
 public:
  /**
   * @brief Construct a new PNG Decoder object.
   *
   * @param display The image to decode the stream into.
   */
  PngDecoder(OnlineImage *image) : ImageDecoder(image), pngle_(pngle_new()) {}
  ~PngDecoder() override { pngle_destroy(this->pngle_); }

  void prepare(uint32_t download_size) override;
  int HOT decode(uint8_t *buffer, size_t size) override;

 protected:
  pngle_t *pngle_;
};

}  // namespace online_image
}  // namespace esphome

#endif  // USE_ONLINE_IMAGE_PNG_SUPPORT
