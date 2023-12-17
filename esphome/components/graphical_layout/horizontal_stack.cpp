#include "horizontal_stack.h"

#include "esphome/components/display/display.h"
#include "esphome/components/display/rect.h"
#include "esphome/core/log.h"

namespace esphome {
namespace graphical_layout {

static const char *TAG = "horizontalstack";

void HorizontalStack::dump_config(int indent_depth, int additional_level_depth) {
  ESP_LOGCONFIG(TAG, "%*sItem Padding: %i", indent_depth, "", this->item_padding_);
  ESP_LOGCONFIG(TAG, "%*sChildren: %i", indent_depth, "", this->children_.size());

  for (LayoutItem *child : this->children_) {
    child->dump_config(indent_depth + additional_level_depth, additional_level_depth);
  }
}

const display::Rect HorizontalStack::measure_item(display::Display *display) {
  display::Rect rect(this->item_padding_, 0, 0, 0);

  for (LayoutItem *child : this->children_) {
    display::Rect child_rect = child->measure_item(display);
    rect.h = std::max(rect.h, child_rect.h);
    rect.w += child_rect.w + this->item_padding_;
  }

  // Add item padding top and bottom
  rect.h += (this->item_padding_ * 2);

  ESP_LOGD(TAG, "Measured size is (%i, %i, %i, %i)", rect.x, rect.y, rect.x2(), rect.y2());

  return rect;
}

void HorizontalStack::render(display::Display *display, display::Rect bounds) {
  int width_offset = this->item_padding_;

  for (LayoutItem *item : this->children_) {
    display::Rect measure = item->measure_item(display);
    display->set_local_coordinates_relative_to_current(width_offset, this->item_padding_);
    item->render(display, measure);
    display->pop_local_coordinates();
    width_offset += measure.w + this->item_padding_;
  }
}

}  // namespace graphical_layout
}  // namespace esphome
