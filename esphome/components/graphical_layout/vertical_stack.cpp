#include "vertical_stack.h"

#include "esphome/components/display/display.h"
#include "esphome/components/display/rect.h"
#include "esphome/core/log.h"

namespace esphome {
namespace graphical_layout {

static const char *const TAG = "verticalstack";

void VerticalStack::dump_config(int indent_depth, int additional_level_depth) {
  ESP_LOGCONFIG(TAG, "%*sItem Padding: %i", indent_depth, "", this->item_padding_);
  ESP_LOGCONFIG(TAG, "%*sChildren: %i", indent_depth, "", this->children_.size());

  for (LayoutItem *child : this->children_) {
    child->dump_config(indent_depth + additional_level_depth, additional_level_depth);
  }
}

display::Rect VerticalStack::measure_item_internal(display::Display *display) {
  display::Rect rect(0, this->item_padding_, 0, 0);

  for (LayoutItem *child : this->children_) {
    display::Rect child_rect = child->measure_item(display);
    rect.w = std::max(rect.w, child_rect.w);
    rect.h += child_rect.h + this->item_padding_;
  }

  // Add item padding left and right
  rect.h += (this->item_padding_ * 2);

  return rect;
}

void VerticalStack::render_internal(display::Display *display, display::Rect bounds) {
  int height_offset = this->item_padding_;

  for (LayoutItem *item : this->children_) {
    display::Rect measure = item->measure_item(display);

    display->set_local_coordinates_relative_to_current(this->item_padding_, height_offset);
    item->render(display, measure);
    display->pop_local_coordinates();
    height_offset += measure.h + this->item_padding_;
  }
}

}  // namespace graphical_layout
}  // namespace esphome
