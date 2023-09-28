#pragma once

#include <cstdarg>
#include <vector>

#include "display.h"
#include "display_color_utils.h"

#include "esphome/core/component.h"
#include "esphome/core/defines.h"

namespace esphome {
namespace display {

class DisplayBuffer : public Display, public PollingComponent {
 public:
  /// Get the width of the image in pixels with rotation applied.
  int get_width() override;
  /// Get the height of the image in pixels with rotation applied.
  int get_height() override;

  /// Set a single pixel at the specified coordinates to the given color.
  void draw_pixel_at(int x, int y, Color color) override;

  /** Given an array of pixels encoded in the nominated format, draw these into the display's buffer.
   * The naive implementation here will work in all cases, but can be overridden by sub-classes
   * in order to optimise the procedure.
   * The parameters describe a rectangular block of pixels, potentially within a larger buffer.
   *
   * \param x_start The starting destination x position
   * \param y_start The starting destination y position
   * \param w the width of the pixel block
   * \param h the height of the pixel block
   * \param ptr A pointer to the start of the data to be copied
   * \param order The ordering of the colors
   * \param bitness Defines the number of bits and their format for each pixel
   * \param big_endian True if 16 bit values are stored big-endian
   * \param x_offset The initial x-offset into the source buffer. Defaults to 0.
   * \param y_offset The initial y-offset into the source buffer. Defaults to 0
   * \param x_pad How many pixels are in each line after the end of the pixels to be copied.
   *
   * The length of each source buffer line will be x_offset + w + x_pad.
   *  TODO: handle one-bit-per-pixel monochrome
   */
  virtual void draw_pixels_in_window(int x_start, int y_start, int w, int h, const uint8_t *ptr, ColorOrder order,
                                     ColorBitness bitness, bool big_endian = true, int x_offset = 0, int y_offset = 0,
                                     int x_pad = 0) {
    if (bitness == COLOR_BITNESS_UNKNOWN)
      return;
    size_t line_stride = x_offset + w + x_pad;  // length of each source line in pixels
    uint32_t color_value;
    for (int y = y_start; y != y_start + h; y++)
      for (int x = x_start; x != x_start + w; x++) {
        size_t source_idx = (y + y_offset) * line_stride + x_offset;
        switch (bitness) {
          default:
            color_value = ptr[source_idx];
            break;
          case COLOR_BITNESS_565:
            source_idx *= 2;
            if (big_endian)
              color_value = (ptr[source_idx] << 8) + ptr[source_idx + 1];
            else
              color_value = ptr[source_idx] + (ptr[source_idx + 1] << 8);
            break;
          case COLOR_BITNESS_888:
            source_idx *= 2;
            if (big_endian)
              color_value = (ptr[source_idx + 0] << 16) + (ptr[source_idx + 1] << 8) + ptr[source_idx + 2];
            else
              color_value = ptr[source_idx + 0] + (ptr[source_idx + 1] << 8) + (ptr[source_idx + 2] << 16);
            break;
        }
        this->draw_pixel_at(x, y, ColorUtil::to_color(color_value, order, bitness));
      }
  }

  virtual int get_height_internal() = 0;
  virtual int get_width_internal() = 0;

 protected:
  virtual void draw_absolute_pixel_internal(int x, int y, Color color) = 0;

  void init_internal_(uint32_t buffer_length);

  uint8_t *buffer_{nullptr};
};

}  // namespace display
}  // namespace esphome
