#pragma once

#include "esphome/components/graphical_layout/graphical_layout.h"
#include "esphome/components/font/font.h"

namespace esphome {
namespace graphical_layout {

const Color COLOR_ON(255, 255, 255, 255);
const Color COLOR_OFF(0, 0, 0, 0);

/** The TextPanel is a UI item that renders a single line of text to a display */
class TextPanel : public LayoutItem {
 public:
  const display::Rect measure_item(display::Display *display);
  void render(display::Display *display, display::Rect bounds);
  void dump_config(int indent_depth, int additional_level_depth);

  void set_item_padding(int item_padding) { this->item_padding_ = item_padding; };
  void set_text(std::string text) { this->text_ = text; };
  void set_font(display::BaseFont *font) { this->font_ = font; };
  void set_foreground_color(Color foreground_color) { this->foreground_color_ = foreground_color; };
  void set_background_color(Color background_color) { this->background_color_ = background_color; };

 protected:
  int item_padding_{0};
  std::string text_{};
  display::BaseFont *font_{nullptr};
  Color foreground_color_{COLOR_ON};
  Color background_color_{COLOR_OFF};
};

}  // namespace graphical_layout
}  // namespace esphome
