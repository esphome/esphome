#include "layout_item.h"

#include "esphome/components/display/display.h"
#include "esphome/components/display/rect.h"

namespace esphome {
namespace graphical_layout {

static const char *const TAG = "layoutitem";

const Color COLOR_OFF(0, 0, 0, 0);
const Color COLOR_ON(255, 255, 255, 255);

display::Rect LayoutItem::measure_item(display::Display *display) {
  display::Rect inner_size = this->measure_item_internal(display);
  return display::Rect(
      0, 0, this->margin_.horizontal() + this->border_.horizontal() + this->padding_.horizontal() + inner_size.w,
      this->margin_.vertical() + this->border_.vertical() + this->padding_.vertical() + inner_size.h);
}

void LayoutItem::render(display::Display *display, display::Rect bounds) {
  // Margin
  display->set_local_coordinates_relative_to_current(this->margin_.left, this->margin_.top);

  // Border
  if (this->border_.any()) {
    display::Rect border_bounds(0, 0, bounds.w - this->margin_.horizontal(), bounds.h - this->margin_.vertical());
    if (this->border_.equals(1)) {
      // Single pixel border use the native function
      display->rectangle(0, 0, border_bounds.w, border_bounds.h, this->border_color_);
    } else {
      // Thicker border need to do mutiple filled rects
      // Top Rectangle
      if (this->border_.top > 0) {
        display->filled_rectangle(border_bounds.x, border_bounds.y, border_bounds.w, this->border_.top);
      }
      // Left Rectangle
      if (this->border_.left > 0) {
        display->filled_rectangle(border_bounds.x, border_bounds.y + this->border_.top, this->border_.left,
                                  border_bounds.h - this->border_.bottom - this->border_.top);
      }
      // Bottom Rectangle
      if (this->border_.bottom > 0) {
        display->filled_rectangle(border_bounds.x, border_bounds.h - this->border_.bottom, border_bounds.w,
                                  this->border_.bottom);
      }
      // Right Rectangle
      if (this->border_.right > 0) {
        display->filled_rectangle(border_bounds.w - this->border_.right, border_bounds.y + this->border_.top,
                                  this->border_.right, border_bounds.h - this->border_.bottom - this->border_.top);
      }
    }
  }

  // Padding
  display->set_local_coordinates_relative_to_current(this->border_.left + this->padding_.left,
                                                     this->border_.top + this->padding_.top);
  display::Rect internal_bounds(
      0, 0, bounds.w - this->margin_.horizontal() - this->border_.horizontal() - this->padding_.horizontal(),
      bounds.h - this->margin_.vertical() - this->border_.vertical() - this->padding_.vertical());

  // Rendering
  this->render_internal(display, internal_bounds);

  // Pop padding  coords
  display->pop_local_coordinates();

  // Border doesn't use local coords

  // Pop margin coords
  display->pop_local_coordinates();
}

void LayoutItem::dump_config_base_properties(const char *tag, int indent_depth) {
  ESP_LOGCONFIG(tag, "%*sMargin: : (L: %i, T: %i, R: %i, B: %i)", indent_depth, "", this->margin_.left,
                this->margin_.top, this->margin_.right, this->margin_.bottom);
  ESP_LOGCONFIG(tag, "%*sBorder: (L: %i, T: %i, R: %i, B: %i)", indent_depth, "", this->border_.left, this->border_.top,
                this->border_.right, this->border_.bottom);
  ESP_LOGCONFIG(tag, "%*sBorder Color: (R: %i, G: %i, B: %i)", indent_depth, "", this->border_color_.r,
                this->border_color_.g, this->border_color_.b);
  ESP_LOGCONFIG(tag, "%*sPadding: : (L: %i, T: %i, R: %i, B: %i)", indent_depth, "", this->padding_.left,
                this->padding_.top, this->padding_.right, this->padding_.bottom);
}

const LogString *horizontal_child_align_to_string(HorizontalChildAlign align) {
  switch (align) {
    case HorizontalChildAlign::LEFT:
      return LOG_STR("LEFT");
    case HorizontalChildAlign::CENTER_HORIZONTAL:
      return LOG_STR("CENTER_HORIZONTAL");
    case HorizontalChildAlign::RIGHT:
      return LOG_STR("RIGHT");
    case HorizontalChildAlign::STRETCH_TO_FIT_WIDTH:
      return LOG_STR("STRETCH_TO_FIT_WIDTH");
    default:
      return LOG_STR("UNKNOWN");
  }
}

const LogString *vertical_child_align_to_string(VerticalChildAlign align) {
  switch (align) {
    case VerticalChildAlign::TOP:
      return LOG_STR("TOP");
    case VerticalChildAlign::CENTER_VERTICAL:
      return LOG_STR("CENTER_VERTICAL");
    case VerticalChildAlign::BOTTOM:
      return LOG_STR("BOTTOM");
    case VerticalChildAlign::STRETCH_TO_FIT_HEIGHT:
      return LOG_STR("STRETCH_TO_FIT_HEIGHT");
    default:
      return LOG_STR("UNKNOWN");
  }
}

}  // namespace graphical_layout
}  // namespace esphome
