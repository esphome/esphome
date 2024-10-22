#pragma once

#include "esphome/components/graphical_layout/layout_item.h"

namespace esphome {
namespace display {
class Display;
class Rect;
}  // namespace display

namespace graphical_layout {

/** The ContainerLayoutItem can be used to derive from when a layout item has children.
 *  It does not define what or how child items get used just that they exist for the item
 */
class ContainerLayoutItem : public LayoutItem {
 public:
  /** Adds an item to this container */
  void add_item(LayoutItem *child) { this->children_.push_back(child); }

  void setup_complete() override {
    for (LayoutItem *child : this->children_) {
      child->setup_complete();
    }
  }

 protected:
  std::vector<LayoutItem *> children_;
};

}  // namespace graphical_layout
}  // namespace esphome
