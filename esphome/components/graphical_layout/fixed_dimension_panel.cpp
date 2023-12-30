#include "fixed_dimension_panel.h"

#include "esphome/components/display/display.h"
#include "esphome/components/display/rect.h"
#include "esphome/core/log.h"

namespace esphome {
namespace graphical_layout {

static const char *const TAG = "fixeddimensionpanel";

void FixedDimensionPanel::dump_config(int indent_depth, int additional_level_depth) {
  this->dump_config_base_properties(TAG, indent_depth);
  if (this->width_.value() < 0) {
    ESP_LOGCONFIG(TAG, "%*sWidth: UNSET (Will use %s's width)", indent_depth, "", 
                  this->unset_width_uses_display_width_ ? "DISPLAY" : "CHILD");
  } else {
    ESP_LOGCONFIG(TAG, "%*sWidth: %i", indent_depth, "", this->width_.value());
  }

if (this->height_.value() < 0) {
    ESP_LOGCONFIG(TAG, "%*sHeight: UNSET (Will use %s's height)", indent_depth, "", 
                  this->unset_height_uses_display_height_ ? "DISPLAY" : "CHILD");
  } else {
    ESP_LOGCONFIG(TAG, "%*sHeight: %i", indent_depth, "", this->height_.value());
  }

  this->child_->dump_config(indent_depth + additional_level_depth, additional_level_depth);
}

display::Rect FixedDimensionPanel::measure_item_internal(display::Display *display) {
  // Call measure_child so they can do any measurements
  display::Rect child_size = this->child_->measure_item(display);
  display::Rect rect(0, 0, this->width_.value(), this->height_.value());

  if (rect.w < 0) {
    if (this->unset_width_uses_display_width_) {
      rect.w = display->get_width();
      // We need to account for our own padding + margin + border
      rect.w -= (this->margin_ + this->border_ + this->padding_) * 2;
    } else {
      rect.w = child_size.w;
    }
  }

  if (rect.h < 0) {
    if (this->unset_height_uses_display_height_) {
      rect.h = display->get_height();
      // We need to account for our own padding + margin + border
      rect.h -= (this->margin_ + this->border_ + this->padding_) * 2;
    } else {
      rect.h = child_size.h;
    }
  }

  return rect;
}

void FixedDimensionPanel::render_internal(display::Display *display, display::Rect bounds) {
  this->child_->render(display, bounds);
}

}  // namespace graphical_layout
}  // namespace esphome
