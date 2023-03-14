#pragma once
#ifdef USE_ARDUINO

#include "esphome/components/display/display_buffer.h"
#include "esphome/components/display/image.h"
#include "esphome/core/helpers.h"
#include "buffer_helper.h"

namespace esphome {
namespace online_image {
/**
 * @brief Format that the image is encoded with.
 */
enum ImageFormat {
  /** JPEG format. Not supported yet. */
  JPEG,
  /** PNG format. */
  PNG,
};

class OnlineImage: public PollingComponent, 
                   public display::Image
{
 public:
  /**
   * @brief Construct a new Online Image object.
   *
   * @param url URL to download the image from.
   * @param width Desired width of the target image area.
   * @param height Desired height of the target image area.
   * @param format Format that the image is encoded in (@see ImageFormat).
   * @param buffer_size Size of the buffer used to download the image.
   */
  OnlineImage(const char *url, uint16_t width, uint16_t height, ImageFormat format, display::ImageType type, uint32_t buffer_size);

  void update() override;

 protected:
  uint8_t *createDefaultImage(uint16_t width, uint16_t height);

  Buffer buffer_;
  const char *url_;
  const uint32_t download_buffer_size_;
  const ImageFormat format_;
};

}  // namespace online_image
}  // namespace esphome

#endif  // USE_ARDUINO
