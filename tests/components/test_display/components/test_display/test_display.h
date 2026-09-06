#pragma once

#include "esphome/components/display/display.h"
#include "esphome/core/color.h"

namespace esphome::test_display {

/** A no-op display that draws nothing and uses no pins.
 *
 * It exists purely to satisfy components that require a display (for example
 * touchscreens, which read the display dimensions) in configurations - most
 * notably YAML build tests - where a real display driver would only get in the
 * way by occupying GPIO pins and pulling in bus dependencies.
 */
class TestDisplay : public display::Display {
 public:
  void update() override { this->do_update_(); }

  void set_dimensions(int width, int height) {
    this->width_ = width;
    this->height_ = height;
  }

  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_COLOR; }

  void draw_pixel_at(int x, int y, Color color) override {}

 protected:
  int get_width_internal() override { return this->width_; }
  int get_height_internal() override { return this->height_; }

  int width_{0};
  int height_{0};
};

}  // namespace esphome::test_display
