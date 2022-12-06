#pragma once

#include "esphome/components/display_menu_base/display_menu_base.h"
#include "esphome/components/display_menu_base/menu_item.h"
#include "esphome/components/display/display_buffer.h"
#include "esphome/core/automation.h"
#include <cstdlib>

namespace esphome {

namespace graphical_display_menu {
struct Position {
  int x;
  int y;
};

struct Dimension {
  int width;
  int height;
};

class GraphicalDisplayMenu : public display_menu_base::DisplayMenuComponent {
 public:
  void setup() override;
  void dump_config() override;

  void set_display_buffer(display::DisplayBuffer *display);
  void set_display_updater(PollingComponent *display_updater);
  void set_font(display::Font *font);
  template<typename V> void set_menu_item_value(V menu_item_value) { this->menu_item_value_ = menu_item_value; }
  void set_foreground_color(Color foreground_color);
  void set_background_color(Color background_color);

 protected:
  void draw_menu() override;
  void draw_item(const display_menu_base::MenuItem *item, uint8_t row, bool selected) override;
  virtual Dimension measure_item(const display_menu_base::MenuItem *item, bool selected);
  virtual void draw_item(const display_menu_base::MenuItem *item, const Position *position,
                         const Dimension *measured_dimensions, bool selected);
  void update() override;

  display::DisplayBuffer *display_buffer_{nullptr};
  PollingComponent *display_updater_{nullptr};
  display::Font *font_{nullptr};
  TemplatableValue<std::string, const display_menu_base::MenuItem *> menu_item_value_{nullptr};
  Color foreground_color_{display::COLOR_ON};
  Color background_color_{display::COLOR_OFF};
};

}  // namespace graphical_display_menu
}  // namespace esphome
