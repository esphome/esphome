#pragma once

namespace esphome {
namespace display {
class Display;
class Rect;
}

namespace graphical_layout {

/** LayoutItem is the base from which all items derive from*/
class LayoutItem {
 public:

  /** Measures the item as it would be drawn on the display and returns the bounds for it
   *
   * param[in] display: Display that will be used for rendering. May be used to help with calculations
   */
  virtual const display::Rect measure_item(display::Display *display) = 0;

  /** Perform the rendering of the item to the display
   *
   * param[in] display: Display to render to
   * param[in] bounds: Size of the area drawing should be constrained to
   */
  virtual void render(display::Display *display, display::Rect bounds) = 0;

  /**
   * param[in] indent_depth: Depth to indent the config
   * param[in] additional_level_depth: If children require their config to be dumped you increment
   *  their indent_depth before calling it
   */
  virtual void dump_config(int indent_depth, int additional_level_depth) = 0;
};

}  // namespace graphical_layout
}  // namespace esphome
