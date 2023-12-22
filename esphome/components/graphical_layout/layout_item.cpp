#include "layout_item.h"

#include "esphome/components/display/display.h"
#include "esphome/components/display/rect.h"

namespace esphome {
namespace graphical_layout {

static const char *const TAG = "layoutitem";

display::Rect LayoutItem::measure_item(display::Display *display) {
  display::Rect inner_size = this->measure_item_internal(display);
  int margin_border_padding = this->margin_ + this->border_ + this->padding_;

  return display::Rect(0, 0, (margin_border_padding * 2) + inner_size.w, (margin_border_padding * 2) + inner_size.h);
}

void LayoutItem::render(display::Display *display, display::Rect bounds) {
  // Margin
  display->set_local_coordinates_relative_to_current(this->margin_, this->margin_);

  // Border
  if (this->border_ > 0) {
    display::Rect border_bounds(0, 0, bounds.w - (this->margin_ * 2), bounds.h - (this->margin_ * 2));
    if (this->border_ == 1) {
      // Single pixel border use the native function
      display->rectangle(0, 0, border_bounds.w, border_bounds.h, this->border_color_);
    } else {
      // Thicker border need to do mutiple filled rects
      // Top rectangle
      display->filled_rectangle(border_bounds.x, border_bounds.y, border_bounds.w, this->border_);
      // Bottom rectangle
      display->filled_rectangle(border_bounds.x, border_bounds.h - this->border_, border_bounds.w, this->border_);
      // Left rectangle
      display->filled_rectangle(border_bounds.x, border_bounds.y, this->border_, border_bounds.h);
      // Right rectangle
      display->filled_rectangle(border_bounds.w - this->border_, border_bounds.y, this->border_, border_bounds.h);
    }
  }

  // Padding
  display->set_local_coordinates_relative_to_current(this->border_ + this->padding_, this->border_ + this->padding_);   
  int margin_border_padding_offset = (this->margin_ + this->border_ + this->padding_) * 2;
  display::Rect internal_bounds(0, 0, bounds.w - margin_border_padding_offset, bounds.h - margin_border_padding_offset);

  // Rendering
  this->render_internal(display, internal_bounds);

  // Pop padding  coords
  display->pop_local_coordinates();

  // Border doesn't use local coords

  // Pop margin coords
  display->pop_local_coordinates();
}


}  // namespace graphical_layout
}  // namespace esphome
