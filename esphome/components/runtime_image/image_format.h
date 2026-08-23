#pragma once

namespace esphome::runtime_image {

/**
 * @brief Image format types that can be decoded dynamically.
 */
enum ImageFormat {
  /** Format is supplied per decode, e.g. detected from the Content-Type header
   *  by online_image; sniffing the image data is not implemented. */
  AUTO,
  /** JPEG format. */
  JPEG,
  /** PNG format. */
  PNG,
  /** BMP format. */
  BMP,
};

}  // namespace esphome::runtime_image
