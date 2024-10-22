#pragma once

#include "esphome/components/graphical_layout/graphical_layout.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace graphical_layout {

/** The FixedDimensionPanel is a UI element which will render a single child with constrained dimensions
 */
class FixedDimensionPanel : public LayoutItem {
 public:
  display::Rect measure_item_internal(display::Display *display) override;
  void render_internal(display::Display *display, display::Rect bounds) override;
  void dump_config(int indent_depth, int additional_level_depth) override;
  void setup_complete() override { this->child_->setup_complete(); }

  void set_child(LayoutItem *child) { this->child_ = child; };
  template<typename V> void set_width(V width) { this->width_ = width; };
  template<typename V> void set_height(V height) { this->height_ = height; }

  void set_unset_width_uses_display_width(bool use_display) { this->unset_width_uses_display_width_ = use_display; }
  void set_unset_height_uses_display_height(bool use_display) { this->unset_height_uses_display_height_ = use_display; }

 protected:
  LayoutItem *child_{nullptr};
  TemplatableValue<int> width_{-1};
  TemplatableValue<int> height_{-1};
  bool unset_width_uses_display_width_{false};
  bool unset_height_uses_display_height_{false};
};

}  // namespace graphical_layout
}  // namespace esphome
