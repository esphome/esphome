#pragma once

#include "esphome/core/color.h"
#include "esphome/core/log.h"

namespace esphome {
namespace display {
class Display;
class Rect;
}  // namespace display

namespace graphical_layout {

extern const Color COLOR_ON;
extern const Color COLOR_OFF;

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

struct Dimension {
  Dimension(){};
  Dimension(int16_t padding) {
    this->left = padding;
    this->top = padding;
    this->right = padding;
    this->bottom = padding;
  }
  Dimension(int16_t left, int16_t top, int16_t right, int16_t bottom) {
    this->left = left;
    this->top = top;
    this->right = right;
    this->bottom = bottom;
  }

  /* Gets the total padding for the horizontal direction (left + right) */
  inline int16_t horizontal() const { return this->left + this->right; };
  /* Gets the total padding for the vertical direction (top + bottom) */
  inline int16_t vertical() const { return this->top + this->bottom; };

  /* Returns true if any value is set to a non-zero value*/
  inline bool any() const {
    return this->left > 0 || this->top > 0 || this->right > 0 || this->bottom > 0;
  };

  /* Returns true if all dimensions are equal to the value */
  inline bool equals(int16_t value) const {
    return this->left == value && this->top == value && this->right == value && this->bottom == value;
  }

  int16_t left{0};
  int16_t top{0};
  int16_t right{0};
  int16_t bottom{0};
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

  /** Dumps the base properties of the LayoutItem. Should be called by implementors dump_config()
   *
   * param[in] tag: Tag to pass to the LOGCONFIG method
   * param[in] indent_depth: Depth to indent the config
   */
  void dump_config_base_properties(const char *tag, int indent_depth);

  /** Called once all setup has been completed (i.e. after code generation and all your set_ methods
   * have been called). Can be used to finalise any configuration
   */
  virtual void setup_complete(){};

  void set_margin(int margin) { this->margin_ = Dimension(margin); };
  void set_margin(int left, int top, int right, int bottom) { this->margin_ = Dimension(left, top, right, bottom); }

  void set_padding(int padding) { this->padding_ = Dimension(padding); };
  void set_padding(int left, int top, int right, int bottom) { this->padding_ = Dimension(left, top, right, bottom); }

  void set_border(int border) { this->border_ = Dimension(border); };
  void set_border(int left, int top, int right, int bottom) { this->border_ = Dimension(left, top, right, bottom); }

  void set_border_color(Color color) { this->border_color_ = color; };

 protected:
  Dimension margin_{};
  Dimension padding_{};
  Dimension border_{};
  Color border_color_{COLOR_ON};
};

const LogString *horizontal_child_align_to_string(HorizontalChildAlign align);
const LogString *vertical_child_align_to_string(VerticalChildAlign align);

}  // namespace graphical_layout
}  // namespace esphome
