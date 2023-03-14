#pragma once
#ifdef USE_ARDUINO

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
   * @param display The display to draw the decoded image to.
   */
  PngDecoder(Buffer *buffer) : ImageDecoder(buffer), pngle(pngle_new()) {}
  virtual ~PngDecoder() { pngle_destroy(pngle); }

  void prepare(WiFiClient *stream) override;
  size_t decode(HTTPClient &http, WiFiClient *stream, std::vector<uint8_t> &buffer) override;

  /**
   * @return The display to draw to. Needed by the callback function.
   */
  const Buffer *buffer() { return buffer_; }

 private:
  pngle_t *pngle;
};

}  // namespace online_image
}  // namespace esphome

#endif  // ONLINE_IMAGE_PNG_SUPPORT

#endif  // USE_ARDUINO
