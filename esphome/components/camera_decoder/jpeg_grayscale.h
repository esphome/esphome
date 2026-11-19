#pragma once

#include <cstddef>
#include <cstdint>

namespace esphome::camera_decoder {

/// ITU-R BT.601 luma weights in 8-bit fixed point. They sum to 256, so a white
/// pixel maps to 255 rather than 254.
constexpr uint16_t LUMA_RED = 77;
constexpr uint16_t LUMA_GREEN = 150;
constexpr uint16_t LUMA_BLUE = 29;

/// Bytes per pixel in the RGB888 layout the decoder produces.
constexpr size_t RGB888_BYTES_PER_PIXEL = 3;

/// Compacts decoded RGB888 pixels to 8-bit luma in place.
///
/// esp_new_jpeg has no one-channel decoder output, so grayscale frames are
/// decoded as RGB888 and reduced here. The integer weights match the
/// conversion used by the camera image processors.
inline void rgb888_to_grayscale_in_place(uint8_t *buffer, size_t pixels) {
  // esp_jpeg_common.h documents RGB888 as R, G, B from low to high address.
  // The destination index is always at most RGB888_BYTES_PER_PIXEL times the
  // source index, so a forward pass never reads a pixel it has overwritten.
  for (size_t i = 0; i < pixels; i++) {
    const uint8_t *pixel = buffer + i * RGB888_BYTES_PER_PIXEL;
    buffer[i] = static_cast<uint8_t>((pixel[0] * LUMA_RED + pixel[1] * LUMA_GREEN + pixel[2] * LUMA_BLUE) >> 8);
  }
}

}  // namespace esphome::camera_decoder
