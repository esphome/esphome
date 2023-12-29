#pragma once

#include <utility>

#include "esphome/components/graphical_layout/graphical_layout.h"
#include "esphome/components/font/font.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace display {

extern const Color COLOR_ON;
extern const Color COLOR_OFF;

}  // namespace display
namespace graphical_layout {

/** The TextPanel is a UI item that renders a single line of text to a display */
class TextPanel : public LayoutItem {
 public:
  display::Rect measure_item_internal(display::Display *display) override;
  void render_internal(display::Display *display, display::Rect bounds) override;
  void dump_config(int indent_depth, int additional_level_depth) override;

  template<typename V> void set_text(V text) { this->text_ = text; };
  void set_font(display::BaseFont *font) { this->font_ = font; };
  void set_foreground_color(Color foreground_color) { this->foreground_color_ = foreground_color; };
  void set_background_color(Color background_color) { this->background_color_ = background_color; };
  void set_text_align(display::TextAlign text_align) { this->text_align_ = text_align; };

 protected:
  TemplatableValue<std::string> text_{};
  display::BaseFont *font_{nullptr};
  display::TextAlign text_align_{display::TextAlign::TOP_LEFT};
  Color foreground_color_{display::COLOR_ON};
  Color background_color_{display::COLOR_OFF};
};

}  // namespace graphical_layout
}  // namespace esphome
