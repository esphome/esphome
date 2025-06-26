#pragma once

#include "esphome/core/component.h"
#include "esphome/components/display_menu_render_base/display_menu_render_base.h"
#include "menu_item.h"

#include <forward_list>

namespace esphome {
namespace display_menu_base {

enum MenuMode {
  MENU_MODE_ROTARY,
  MENU_MODE_JOYSTICK,
};

class MenuItem;
class MenuItemMenu;

using namespace display_menu_render_base;

/** Class to display a hierarchical menu.
 *
 */
class DisplayMenuComponent : public Component {
 public:
  void setup() override;
  void set_root_item(MenuItemMenu *item) { this->displayed_item_ = this->root_item_ = item; }
  void set_active(bool active) { this->active_ = active; }
  void set_mode(MenuMode mode) { this->mode_ = mode; }

  /** Set whether the "right" input should be used to enter menu options.
   * @param opt True to enable "right" input for menu entry, false to disable.
   */
  void set_right_for_menu_enter_opt(bool opt) { this->right_for_menu_enter_opt_ = opt; }
  void set_rows(uint8_t rows) { this->rows_ = rows; }

  float get_setup_priority() const override { return setup_priority::PROCESSOR; }

  void up();
  void down();
  void left();
  void right();
  void enter();
  void back();

  // Go to root of menu and show it
  void show_main();
  // Reset menu to root item
  void reset_menu();
  // Check that current item is root
  bool is_at_main() const { return this->displayed_item_ == this->root_item_; }

  void show();
  void hide();

  void draw();

  bool is_active() const { return this->active_; }

  void add_render(MenuRenderInterface *render) { this->renders_.push_back(render); }

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

  void recurse_menu_items_(MenuItemMenu *parent_menu);
  void generate_to_menu_items_(MenuItemMenu *menu);
  size_t process_group_(MenuItemMenu *menu, groups::Group *group);

  virtual void on_before_show(){};
  virtual void on_after_show(){};
  virtual void on_before_hide(){};
  virtual void on_after_hide(){};

  uint8_t rows_;
  bool active_;
  MenuMode mode_;
  bool right_for_menu_enter_opt_{true};
  MenuItemMenu *root_item_{nullptr};

  MenuItemMenu *displayed_item_{nullptr};
  uint8_t top_index_{0};
  uint8_t cursor_index_{0};
  std::forward_list<std::pair<uint8_t, uint8_t>> selection_stack_{};
  bool editing_{false};
  bool root_on_enter_called_{false};

  std::vector<MenuRenderInterface *> renders_;
};

}  // namespace display_menu_base
}  // namespace esphome
