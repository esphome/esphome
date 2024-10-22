#include "display_rendering_panel.h"

#include "esphome/components/display/display.h"
#include "esphome/components/display/rect.h"
#include "esphome/core/log.h"

namespace esphome {
namespace graphical_layout {

static const char *const TAG = "displayrenderingpanel";

void DisplayRenderingPanel::dump_config(int indent_depth, int additional_level_depth) {
  this->dump_config_base_properties(TAG, indent_depth);
  ESP_LOGCONFIG(TAG, "%*sDimensions: %ix%i", indent_depth, "", this->width_.value(), this->height_.value());
  ESP_LOGCONFIG(TAG, "%*sHas drawing lambda: %s", indent_depth, "", YESNO(this->lambda_ != nullptr));
}

display::Rect DisplayRenderingPanel::measure_item_internal(display::Display *display) {
  return display::Rect(0, 0, this->width_.value(), this->height_.value());
}

void DisplayRenderingPanel::render_internal(display::Display *display, display::Rect bounds) {
  this->lambda_(*display, bounds);
}

}  // namespace graphical_layout
}  // namespace esphome
