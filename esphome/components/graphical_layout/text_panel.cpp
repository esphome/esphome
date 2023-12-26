#include "text_panel.h"

#include "esphome/components/display/display.h"
#include "esphome/components/display/rect.h"
#include "esphome/core/log.h"

namespace esphome {
namespace graphical_layout {

static const char *const TAG = "textpanel";
static const int TEXT_ALIGN_X_MASK = (int)display::TextAlign::RIGHT | (int)display::TextAlign::CENTER_HORIZONTAL;
static const int TEXT_ALIGN_Y_MASK = (int)display::TextAlign::BOTTOM | (int)display::TextAlign::BASELINE | (int)display::TextAlign::CENTER_VERTICAL;

void TextPanel::dump_config(int indent_depth, int additional_level_depth) {
  ESP_LOGCONFIG(TAG, "%*sText: %s", indent_depth, "", this->text_.c_str());
}

display::Rect TextPanel::measure_item_internal(display::Display *display) {
  int x1;
  int y1;
  int width;
  int height;
  display->get_text_bounds(0, 0, this->text_.c_str(), this->font_, this->text_align_, &x1, &y1, &width,
                           &height);
  return display::Rect(0, 0, width, height);
}

void TextPanel::render_internal(display::Display *display, display::Rect bounds) {
  int width, height, x_offset, baseline;

  this->font_->measure(this->text_.c_str(), &width, &x_offset, &baseline, &height);
  
  const auto x_align = display::TextAlign(int(this->text_align_) & TEXT_ALIGN_X_MASK);
  const auto y_align = display::TextAlign(int(this->text_align_) & TEXT_ALIGN_Y_MASK);

  display::Rect text_bounds(0, 0, bounds.w, bounds.h);

  switch (x_align) {
    case display::TextAlign::RIGHT: {
        bounds.x = bounds.w - width;
      break;
    }
    case display::TextAlign::CENTER_HORIZONTAL: {
      bounds.x = (bounds.w - width) / 2;
      break;
    }
    case display::TextAlign::LEFT:
    default: {
      // LEFT
      bounds.x = 0;
      break;
    }
  }

  switch (y_align) {
    case display::TextAlign::BOTTOM: {
      bounds.y = bounds.h - height;
      break;
    }
    case display::TextAlign::BASELINE: {
      bounds.y = (bounds.h  - height) + baseline;
      break;
    }
    case display::TextAlign::CENTER_VERTICAL: {
      bounds.y = (bounds.h - height) / 2;
      break;
    }
    case display::TextAlign::TOP:
    default: {
      bounds.y = 0;
      break;
    }
  }

  auto rendered_alignment = display::TextAlign::TOP_LEFT;
  display->print(bounds.x, bounds.y, this->font_, this->foreground_color_, rendered_alignment, this->text_.c_str());

}

}  // namespace graphical_layout
}  // namespace esphome
