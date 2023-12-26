#pragma once

#include "esphome/core/color.h"

namespace esphome {
namespace display {
class Display;
class Rect;
}  // namespace display

namespace graphical_layout {

/* HorizontalChildAlign is used to control alignment of children horizontally */
enum class HorizontalChildAlign {
  /* Aligns all children to the left of their available width */
  LEFT = 0x00,

  /* Aligns all children to the center of the available width */
  CENTER_HORIZONTAL = 0x01,

  /* Aligns all children to the right of the available width */
  RIGHT = 0x02,

  /* Regardless of the requested size of a child they will be given the entire width of their parent */
  STRETCH_TO_FIT_WIDTH = 0x03
};

/* VerticalChildAlign is used to control alignment of children vertically */
enum class VerticalChildAlign {
  /* Aligns all children to the top of the available height */
  TOP = 0x00,

  /* Aligns all children with the center of the available height */
  CENTER_VERTICAL = 0x01,

  /* Aligns all children to the bottom of the available height*/
  BOTTOM = 0x02,

  /* Regardless of the requested size of a child they will be given the entire height of their parent */
  STRETCH_TO_FIT_HEIGHT = 0x03
};

/** LayoutItem is the base from which all items derive from*/
class LayoutItem {
 public:
  /** Measures the item as it would be drawn on the display and returns the bounds for it. This should
   * include any margin and padding. It is rare you will need to override this unless you are doing
   * something non-standard with margins and padding
   *
   * param[in] display: Display that will be used for rendering. May be used to help with calculations
   */
  virtual display::Rect measure_item(display::Display *display);

  /** Measures the internal size of the item this should only be the portion drawn exclusive
   * of any padding or margins
   * 
   * param[in] display: Display that will be used for rendering. May be used to help with calculations
  */
  virtual display::Rect measure_item_internal(display::Display *display) = 0;

  /** Perform the rendering of the item to the display accounting for the margin and padding of the
   * item. It is rare you will need to override this unless you are doing something non-standard with
   * margins and padding
   *
   * param[in] display: Display to render to
   * param[in] bounds: Size of the area drawing should be constrained to
   */
  virtual void render(display::Display *display, display::Rect bounds);

  /** Performs the rendering of the item internals of the item exclusive of any padding or margins
   * (or rather, after they've already been handled by render)
   * 
   * param[in] display: Display to render to
   * param[in] bounds: Size of the area drawing should be constrained to
  */
  virtual void render_internal(display::Display *display, display::Rect bounds) = 0;

  /** Dump the items config to aid the user
   * 
   * param[in] indent_depth: Depth to indent the config
   * param[in] additional_level_depth: If children require their config to be dumped you increment
   *  their indent_depth before calling it
   */
  virtual void dump_config(int indent_depth, int additional_level_depth) = 0;

  void set_margin(int margin) { this->margin_ = margin; };
  void set_padding(int padding) { this->padding_ = padding; };
  void set_border(int border) { this->border_ = border; };
  void set_border_color(Color color) { this->border_color_ = color; };

 protected:
  int margin_{0};
  int padding_{0};
  int border_{0};
  Color border_color_{Color(0, 0, 0, 0)};
};

}  // namespace graphical_layout
}  // namespace esphome
