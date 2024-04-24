#pragma once

#include "esphome/core/color.h"
#include "esphome/components/display_menu_base/display_menu_base.h"
#include "esphome/components/display_menu_base/menu_item.h"
#include "esphome/core/automation.h"
#include <cstdlib>

namespace esphome {

// forward declare from display namespace
namespace display {
class Display;
class DisplayPage;
class BaseFont;
class Rect;
}  // namespace display

namespace graphical_display_menu {

const Color COLOR_ON(255, 255, 255, 255);
const Color COLOR_OFF(0, 0, 0, 0);

struct MenuItemValueArguments {
  MenuItemValueArguments(const display_menu_base::MenuItem *item, bool is_item_selected, bool is_menu_editing) {
    this->item = item;
    this->is_item_selected = is_item_selected;
    this->is_menu_editing = is_menu_editing;
  }

  const display_menu_base::MenuItem *item;
  bool is_item_selected;
  bool is_menu_editing;
};

class GraphicalDisplayMenu : public display_menu_base::DisplayMenuComponent {
 public:
  void setup() override;
  void dump_config() override;

  void set_display(display::Display *display);
  void set_font(display::BaseFont *font);
  template<typename V> void set_menu_item_value(V menu_item_value) { this->menu_item_value_ = menu_item_value; }
  void set_foreground_color(Color foreground_color);
  void set_background_color(Color background_color);

  void add_on_redraw_callback(std::function<void()> &&cb) { this->on_redraw_callbacks_.add(std::move(cb)); }

  void draw(display::Display *display, const display::Rect *bounds);

 protected:
  void draw_and_update() override;
  void draw_menu() override;
  void draw_menu_internal_(display::Display *display, const display::Rect *bounds);
  void draw_item(const display_menu_base::MenuItem *item, uint8_t row, bool selected) override;
  virtual display::Rect measure_item(display::Display *display, const display_menu_base::MenuItem *item,
                                     const display::Rect *bounds, bool selected);
  virtual void draw_item(display::Display *display, const display_menu_base::MenuItem *item,
                         const display::Rect *bounds, bool selected);
  void update() override;

  void on_before_show() override;
  void on_before_hide() override;

  std::unique_ptr<display::DisplayPage> display_page_{nullptr};
  const display::DisplayPage *previous_display_page_{nullptr};
  display::Display *display_{nullptr};
  display::BaseFont *font_{nullptr};
  TemplatableValue<std::string, const MenuItemValueArguments *> menu_item_value_;
  Color foreground_color_{COLOR_ON};
  Color background_color_{COLOR_OFF};

  CallbackManager<void()> on_redraw_callbacks_{};
};

class GraphicalDisplayMenuOnRedrawTrigger : public Trigger<const GraphicalDisplayMenu *> {
 public:
  explicit GraphicalDisplayMenuOnRedrawTrigger(GraphicalDisplayMenu *parent) {
    parent->add_on_redraw_callback([this, parent]() { this->trigger(parent); });
  }
};

}  // namespace graphical_display_menu
}  // namespace esphome
