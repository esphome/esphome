#pragma once

#include "esphome/core/component.h"

#include "menu_item.h"

#include <forward_list>

namespace esphome {
namespace display_menu_base {

enum MenuMode {
  MENU_MODE_ROTARY,
  MENU_MODE_JOYSTICK,
};

class MenuItem;

/** Class to display a hierarchical menu.
 *
 */
class DisplayMenuComponent : public Component {
 public:
  void set_root_item(MenuItemMenu *item) { this->displayed_item_ = this->root_item_ = item; }
  void set_active(bool active) { this->active_ = active; }
  void set_mode(MenuMode mode) { this->mode_ = mode; }
  void set_rows(uint8_t rows) { this->rows_ = rows; }

  float get_setup_priority() const override { return setup_priority::PROCESSOR; }

  void up();
  void down();
  void left();
  void right();
  void enter();

  void show_main();
  void show();
  void hide();

  void draw();

  bool is_active() const { return this->active_; }

 protected:
  void reset_();
  void process_initial_();
  bool check_healthy_and_active_();
  MenuItem *get_selected_item_() { return this->displayed_item_->get_item(this->cursor_index_); }
  bool cursor_up_();
  bool cursor_down_();
  bool enter_menu_();
  bool leave_menu_();
  void finish_editing_();
  virtual void draw_menu();
  virtual void draw_item(const MenuItem *item, uint8_t row, bool selected) = 0;
  virtual void update() {}
  virtual void draw_and_update() {
    draw_menu();
    update();
  }

  virtual void on_before_show(){};
  virtual void on_after_show(){};
  virtual void on_before_hide(){};
  virtual void on_after_hide(){};

  uint8_t rows_;
  bool active_;
  MenuMode mode_;
  MenuItemMenu *root_item_{nullptr};

  MenuItemMenu *displayed_item_{nullptr};
  uint8_t top_index_{0};
  uint8_t cursor_index_{0};
  std::forward_list<std::pair<uint8_t, uint8_t>> selection_stack_{};
  bool editing_{false};
  bool root_on_enter_called_{false};
};

}  // namespace display_menu_base
}  // namespace esphome
