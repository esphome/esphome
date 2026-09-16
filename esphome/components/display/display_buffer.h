#pragma once

#include <cstdarg>
#include <vector>

#include "display.h"
#include "esphome/core/helpers.h"
#include "display_color_utils.h"

#include "esphome/core/component.h"
#include "esphome/core/defines.h"

namespace esphome::display {

class DisplayBuffer : public Display {
 public:
  /// Get the width of the image in pixels with rotation applied.
  int get_width() override;
  /// Get the height of the image in pixels with rotation applied.
  int get_height() override;

  /// Set a single pixel at the specified coordinates to the given color.
  void draw_pixel_at(int x, int y, Color color) override;

 protected:
  virtual void draw_absolute_pixel_internal(int x, int y, Color color) = 0;

  void init_internal_(uint32_t buffer_length);

  /// Internal size read once through the virtual accessors; the rotated
  /// branches of draw_pixel_at() need it on every pixel. Every driver
  /// finalises its size before its first draw (the ones that clear during
  /// setup size themselves first), so the zero test only covers the cold read.
  int ESPHOME_ALWAYS_INLINE internal_width_() {
    if (this->width_internal_ == 0)
      this->width_internal_ = this->get_width_internal();
    return this->width_internal_;
  }
  int ESPHOME_ALWAYS_INLINE internal_height_() {
    if (this->height_internal_ == 0)
      this->height_internal_ = this->get_height_internal();
    return this->height_internal_;
  }

  uint8_t *buffer_{nullptr};
  int width_internal_{0};
  int height_internal_{0};
};

}  // namespace esphome::display
