#pragma once

#include <cstdarg>
#include <vector>

#include "display.h"
#include "display_color_utils.h"

#include "esphome/core/component.h"
#include "esphome/core/defines.h"

namespace esphome {
namespace display {

class DisplayBuffer : public Display {
 public:
  /// Get the width of the image in pixels with rotation applied.
  int get_width() override;
  /// Get the height of the image in pixels with rotation applied.
  int get_height() override;

  /// Set a single pixel at the specified coordinates to the given color.
  void draw_pixel_at(int x, int y, Color color) override;

  /// Draw directly to the display buffer in 8-pixel high groups, y_row = y / 8
  void draw_vertical_pixel_group(int x, int y_row, uint8_t colors, uint8_t transparency,
                                 bool reverse_bit_order) override;
  /// Draw directly to the display buffer in 8-pixel wide groups, x_column = x / 8
  void draw_horizontal_pixel_group(int x_column, int y, uint8_t colors, uint8_t transparency,
                                   bool reverse_bit_order) override;
  /// Get supported mode (vertical, horizontal, none), can change depending on display rotation
  PixelGroupMode get_pixel_group_mode() override;

  static uint8_t reverse_bit_order(uint8_t data);

 protected:
  virtual void draw_absolute_pixel_internal(int x, int y, Color color) = 0;

  virtual void draw_vertical_pixel_group_internal(int x, int y_row, uint8_t colors, uint8_t transparency,
                                                  bool reverse_bit_order) {}
  virtual void draw_horizontal_pixel_group_internal(int x_column, int y, uint8_t colors, uint8_t transparency,
                                                    bool reverse_bit_order) {}
  virtual inline PixelGroupMode get_pixel_group_mode_internal() { return PIXEL_GROUP_NONE; }

  void init_internal_(uint32_t buffer_length);

  uint8_t *buffer_{nullptr};
};

}  // namespace display
}  // namespace esphome
