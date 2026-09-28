#pragma once

#include <optional>

namespace esphome::runtime_image {

/**
 * @brief Image format types that can be decoded dynamically.
 */
enum ImageFormat {
  /** Format is supplied per decode, e.g. detected from the Content-Type header
   *  by online_image; sniffing the image data is not implemented. */
  AUTO,
  /** BMP format. */
  BMP,
  /** JPEG format. */
  JPEG,
  /** PNG format. */
  PNG,
  /** QOI format. */
  QOI,
};

/// Canonical MIME type for a format; "image/*" for AUTO/unknown
const char *get_mime_type_for_format(ImageFormat format);
/// Case-insensitive substring match of known media types; nullopt if none found
std::optional<ImageFormat> get_format_for_mime_type(const char *mime_type);

}  // namespace esphome::runtime_image
