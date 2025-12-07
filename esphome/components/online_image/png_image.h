#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "image_decoder.h"
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
  PngDecoder(OnlineImage *image);
  ~PngDecoder() override;

  int prepare(size_t download_size) override;
  int HOT decode(uint8_t *buffer, size_t size) override;

  void increment_pixels_decoded(uint32_t count) { this->pixels_decoded_ += count; }
  uint32_t get_pixels_decoded() const { return this->pixels_decoded_; }

 protected:
  RAMAllocator<pngle_t> allocator_;
  pngle_t *pngle_;
  uint32_t pixels_decoded_{0};
};

}  // namespace online_image
}  // namespace esphome

#endif  // USE_ONLINE_IMAGE_PNG_SUPPORT
