#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace esphome::camera_image {

/// Crop rectangle, rotation and output size of a camera image, fixed by code generation.
struct Transform {
  /// Scratch frame for a rotation; nullptr when no rotation is configured or it could not be allocated.
  uint8_t *buffer;
  size_t buffer_size;
  /// Crop rectangle; a zero width means no crop is configured.
  int16_t crop_x;
  int16_t crop_y;
  int16_t crop_width;
  int16_t crop_height;
  /// Size of the frame after the crop and rotation.
  int16_t output_width;
  int16_t output_height;
  /// 0, 90, 180 or 270 degrees.
  uint16_t rotation;
};

/// Rotates a crop rectangle into `output`. The pixel size is a template parameter, so every copy has a compile-time
/// length the compiler inlines instead of calling memcpy once per pixel with a run-time size.
template<size_t PIXEL_SIZE>
void rotate_crop_into(uint8_t *output, const uint8_t *crop, int crop_width, int crop_height, int output_width,
                      int output_height, size_t source_stride, size_t output_stride, int rotation) {
  for (int y = 0; y < output_height; y++) {
    for (int x = 0; x < output_width; x++) {
      size_t source_row;
      size_t source_column;
      switch (rotation) {
        case 90:
          source_row = static_cast<size_t>(crop_height - 1 - x);
          source_column = static_cast<size_t>(y);
          break;
        case 180:
          source_row = static_cast<size_t>(crop_height - 1 - y);
          source_column = static_cast<size_t>(crop_width - 1 - x);
          break;
        default:  // 270
          source_row = static_cast<size_t>(x);
          source_column = static_cast<size_t>(crop_width - 1 - y);
          break;
      }
      std::memcpy(output + static_cast<size_t>(y) * output_stride + static_cast<size_t>(x) * PIXEL_SIZE,
                  crop + source_row * source_stride + source_column * PIXEL_SIZE, PIXEL_SIZE);
    }
  }
}

/// Rotates by 90, 180 or 270 degrees, dispatching once so each loop copies a fixed-size pixel.
/// Returns false for any other rotation or for a pixel size other than 1, 2 or 3 bytes.
inline bool rotate_crop(size_t pixel_size, uint8_t *output, const uint8_t *crop, int crop_width, int crop_height,
                        int output_width, int output_height, size_t source_stride, size_t output_stride, int rotation) {
  if (rotation != 90 && rotation != 180 && rotation != 270)
    return false;
  switch (pixel_size) {
    case 1:
      rotate_crop_into<1>(output, crop, crop_width, crop_height, output_width, output_height, source_stride,
                          output_stride, rotation);
      return true;
    case 2:
      rotate_crop_into<2>(output, crop, crop_width, crop_height, output_width, output_height, source_stride,
                          output_stride, rotation);
      return true;
    case 3:
      rotate_crop_into<3>(output, crop, crop_width, crop_height, output_width, output_height, source_stride,
                          output_stride, rotation);
      return true;
    default:
      return false;
  }
}

}  // namespace esphome::camera_image
