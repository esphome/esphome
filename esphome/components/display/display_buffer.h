#pragma once

#include <cstdarg>
#include <vector>

#include "display.h"
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
  void cache_internal_size_();

  uint8_t *buffer_{nullptr};
  // Internal size for the rotated branches of draw_pixel_at(), filled by
  // init_internal_() or on the first rotated pixel. There is no invalidation:
  // a driver must not change its internal size after its first draw.
  int width_internal_{0};
  int height_internal_{0};
};

}  // namespace esphome::display
