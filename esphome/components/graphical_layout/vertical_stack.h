#pragma once

#include "esphome/components/graphical_layout/graphical_layout.h"
#include "esphome/components/graphical_layout/container_layout_item.h"

namespace esphome {
namespace graphical_layout {

/** The HorizontalStack is a UI element which will render a series of items top to bottom down a display
 */
class VerticalStack : public ContainerLayoutItem {
 public:
  const display::Rect measure_item(display::Display *display);
  void render(display::Display *display, display::Rect bounds);

  void dump_config(int indent_depth, int additional_level_depth);
  void set_item_padding(int item_padding) { this->item_padding_ = item_padding; };

 protected:
  int item_padding_{0};
};

}  // namespace graphical_layout
}  // namespace esphome
