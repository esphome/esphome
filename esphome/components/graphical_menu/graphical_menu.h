#pragma once

#include "esphome/components/display/display_buffer.h"
#include "esphome/components/ssd1306_base/ssd1306_base.h"
#include "esphome/components/display_menu_base/display_menu_base.h"
#include "esphome/core/color.h"

#include <forward_list>
#include <vector>

namespace esphome {
namespace graphical_menu {

/** Class to display a hierarchical menu.
 *
 */
class GraphicalMenuComponent : public display_menu_base::DisplayMenuComponent {
 public:
  void set_display(ssd1306_base::SSD1306 *display) { this->display_ = display; }
  void set_dimensions(uint8_t columns, uint8_t rows) {
    this->columns_ = columns;
    set_rows(rows);
  }
  void set_mark_selected(uint8_t c) { this->mark_selected_ = c; }
  void set_mark_editing(uint8_t c) { this->mark_editing_ = c; }
  void set_mark_submenu(uint8_t c) { this->mark_submenu_ = c; }
  void set_mark_back(uint8_t c) { this->mark_back_ = c; }
  void set_font(display::Font *font) { this->font_ = font; }
  void set_color(Color val) { this->color_ = val; }

  void setup() override;
  float get_setup_priority() const override;

  void dump_config() override;

 protected:
  void draw_item(const display_menu_base::MenuItem *item, uint8_t row, bool selected) override;
  void update() override { this->display_->update(); }

  ssd1306_base::SSD1306 *display_;
  uint8_t columns_;
  char mark_selected_;
  char mark_editing_;
  char mark_submenu_;
  char mark_back_;
  display::Font *font_;
  Color color_{Color::BLACK};
};

}  // namespace graphical_menu
}  // namespace esphome
