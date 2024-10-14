#pragma once

#include "esphome/components/graphical_layout/graphical_layout.h"
#include "esphome/components/graphical_layout/container_layout_item.h"

namespace esphome {
namespace graphical_layout {

/** The HorizontalStack is a UI element which will render a series of items top to bottom down a display
 */
class VerticalStack : public ContainerLayoutItem {
 public:
  display::Rect measure_item_internal(display::Display *display) override;
  void render_internal(display::Display *display, display::Rect bounds) override;
  void dump_config(int indent_depth, int additional_level_depth) override;

  void set_item_padding(int item_padding) { this->item_padding_ = item_padding; };
  void set_child_align(HorizontalChildAlign child_align) { this->child_align_ = child_align; };

 protected:
  int item_padding_{0};
  HorizontalChildAlign child_align_{HorizontalChildAlign::LEFT};
};

}  // namespace graphical_layout
}  // namespace esphome
