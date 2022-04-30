#pragma once

#include "esphome/components/number/number.h"
#include "esphome/components/select/select.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/lcd_base/lcd_display.h"
#include "esphome/components/display_menu_base/display_menu_base.h"
#include "esphome/core/component.h"

#include <forward_list>
#include <vector>

namespace esphome {
namespace lcd_menu {

/** Class to display a hierarchical menu.
 *
 */
class LCDCharacterMenuComponent : public display_menu_base::DisplayMenuComponent {
 public:
  void set_display(lcd_base::LCDDisplay *display) { this->display_ = display; }
  void set_dimensions(uint8_t columns, uint8_t rows) {
    this->columns_ = columns;
    set_rows(rows);
  }
  void set_mark_selected(uint8_t c) { this->mark_selected_ = c; }
  void set_mark_editing(uint8_t c) { this->mark_editing_ = c; }
  void set_mark_submenu(uint8_t c) { this->mark_submenu_ = c; }
  void set_mark_back(uint8_t c) { this->mark_back_ = c; }

  void dump_config() override;

 protected:
  virtual void draw_item_(const display_menu_base::MenuItem *item, uint8_t row, bool selected);
  virtual void update_() {
    this->display_->update();
  }

  lcd_base::LCDDisplay *display_;
  uint8_t columns_;
  char mark_selected_;
  char mark_editing_;
  char mark_submenu_;
  char mark_back_;
};

}  // namespace lcd_menu
}  // namespace esphome
