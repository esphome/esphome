#pragma once

#include "image_decoder.h"
#ifdef ONLINE_IMAGE_PNG_SUPPORT
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
  PngDecoder(OnlineImage *image) : ImageDecoder(image), pngle(pngle_new()) {}
  virtual ~PngDecoder() { pngle_destroy(pngle); }

  void prepare(uint32_t download_size) override;
  int HOT decode(uint8_t *buffer, size_t size) override;

 private:
  pngle_t *pngle;
};

}  // namespace online_image
}  // namespace esphome

#endif  // ONLINE_IMAGE_PNG_SUPPORT
