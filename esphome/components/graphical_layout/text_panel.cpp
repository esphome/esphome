#include "text_panel.h"

#include "esphome/components/display/display.h"
#include "esphome/components/display/rect.h"
#include "esphome/core/log.h"

namespace esphome {
namespace graphical_layout {

static const char *const TAG = "textpanel";

void TextPanel::dump_config(int indent_depth, int additional_level_depth) {
  ESP_LOGCONFIG(TAG, "%*sText: %s", indent_depth, "", this->text_.c_str());
}

const display::Rect TextPanel::measure_item(display::Display *display) {
  int x1;
  int y1;
  int width;
  int height;

  display->get_text_bounds(0, 0, this->text_.c_str(), this->font_, display::TextAlign::TOP_LEFT, &x1, &y1, &width,
                           &height);

  return display::Rect(0, 0, width, height);
}

void TextPanel::render(display::Display *display, display::Rect bounds) {
  display->print(0, 0, this->font_, this->foreground_color_, display::TextAlign::TOP_LEFT, this->text_.c_str());
}

}  // namespace graphical_layout
}  // namespace esphome
