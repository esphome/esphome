#pragma once

namespace esphome::runtime_image {

/**
 * @brief Image format types that can be decoded dynamically.
 */
enum ImageFormat {
  /** Automatically detect from data. Not implemented yet. */
  AUTO,
  /** JPEG format. */
  JPEG,
  /** PNG format. */
  PNG,
  /** BMP format. */
  BMP,
};

}  // namespace esphome::runtime_image
