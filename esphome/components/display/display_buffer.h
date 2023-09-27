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

  /// Given a buffer of pixels encoded in the nominated format, draw these into the display.
  /// The naive implementation here will work in all cases, but can be overridden by sub-classes
  /// in order to optimise the procedure.
  /// TODO: handle one-bit-per-pixel monochrome
  virtual void draw_pixels_in_window(int x_start, int y_start, int x_end, int y_end, const uint8_t *ptr,
                                     ColorOrder order, ColorBitness bitness, bool big_endian = true) {
    uint32_t color_value;
    for (int y = y_start; y != y_end; y++)
      for (int x = x_start; x != x_end; x++) {
        switch (bitness) {
          case COLOR_BITNESS_332:
            color_value = *ptr++;
            break;
          case COLOR_BITNESS_565:
            if (big_endian)
              color_value = (ptr[0] << 8) + ptr[1];
            else
              color_value = ptr[0] + (ptr[1] << 8);
            ptr += 2;
            break;
          case COLOR_BITNESS_888:
            if (big_endian)
              color_value = (ptr[0] << 16) + (ptr[1] << 8) + ptr[2];
            else
              color_value = ptr[0] + (ptr[1] << 8) + (ptr[2] << 16);
            ptr += 3;
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
