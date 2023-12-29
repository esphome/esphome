#include "fixed_dimension_panel.h"

#include "esphome/components/display/display.h"
#include "esphome/components/display/rect.h"
#include "esphome/core/log.h"

namespace esphome {
namespace graphical_layout {

static const char *const TAG = "fixeddimensionpanel";

void FixedDimensionPanel::dump_config(int indent_depth, int additional_level_depth) {
  ESP_LOGCONFIG(TAG, "%*sWidth: %i (Will use display width: %s)", indent_depth, "", this->width_.value(),
                YESNO(this->width_.value() < 1));
  ESP_LOGCONFIG(TAG, "%*sHeight: %i (Will use display height: %s)", indent_depth, "", this->height_.value(),
                YESNO(this->height_.value() < 1));
  this->child_->dump_config(indent_depth + additional_level_depth, additional_level_depth);
}

display::Rect FixedDimensionPanel::measure_item_internal(display::Display *display) {
  display::Rect rect(0, 0, this->width_.value(), this->height_.value());
  if (rect.w < 1) {
    rect.w = display->get_width();
  }
  if (rect.h < 1) {
    rect.h = display->get_height();
  }
  // Call measure_child just so they can do any measurements
  this->child_->measure_item(display);

  return rect;
}

void FixedDimensionPanel::render_internal(display::Display *display, display::Rect bounds) {
  this->child_->render_internal(display, bounds);
}

}  // namespace graphical_layout
}  // namespace esphome
