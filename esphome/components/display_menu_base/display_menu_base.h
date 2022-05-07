#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/automation.h"

#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif

#include <forward_list>
#include <vector>

namespace esphome {
namespace display_menu_base {

enum MenuItemType {
  MENU_ITEM_ROOT,
  MENU_ITEM_LABEL,
  MENU_ITEM_MENU,
  MENU_ITEM_BACK,
  MENU_ITEM_SELECT,
  MENU_ITEM_NUMBER,
  MENU_ITEM_SWITCH,
  MENU_ITEM_COMMAND,
};

class MenuItem;
class DisplayMenuOnEnterTrigger;
class DisplayMenuOnLeaveTrigger;
class DisplayMenuOnValueTrigger;

using item_writer_t = std::function<std::string(const MenuItem *)>;

class MenuItem {
 public:
  explicit MenuItem(MenuItemType t = MENU_ITEM_ROOT) : item_type_(t) {}
  void set_parent(MenuItem *parent) { this->parent_ = parent; }
  MenuItem *get_parent() { return this->parent_; }
  MenuItemType get_type() const { return this->item_type_; }
  void add_item(MenuItem *item) {
    item->set_parent(this);
    this->items_.push_back(item);
  }
  template<typename V> void set_text(V val) { this->text_ = val; }
#ifdef USE_SELECT
  void set_select_variable(select::Select *var) { this->select_var_ = var; }
#endif
#ifdef USE_NUMBER
  void set_number_variable(number::Number *var) { this->number_var_ = var; }
  void set_format(const std::string &fmt) { this->format_ = fmt; }
#endif
#ifdef USE_SWITCH
  void set_switch_variable(switch_::Switch *var) { this->switch_var_ = var; }
  void set_on_text(const std::string &t) { this->switch_on_text_ = t; }
  void set_off_text(const std::string &t) { this->switch_off_text_ = t; }
#endif
  void set_immediate_edit(bool val) { this->immediate_edit_ = val; }
  void add_on_enter_callback(std::function<void()> &&cb) { this->on_enter_callbacks_.add(std::move(cb)); }
  void add_on_leave_callback(std::function<void()> &&cb) { this->on_leave_callbacks_.add(std::move(cb)); }
  void add_on_value_callback(std::function<void()> &&cb) { this->on_value_callbacks_.add(std::move(cb)); }

  size_t items_size() const { return this->items_.size(); }
  MenuItem *get_item(size_t i) { return this->items_[i]; }

  bool get_immediate_edit() const { return this->immediate_edit_; }

  std::string get_text() const { return const_cast<MenuItem *>(this)->text_.value(this); }
  bool has_value() const;
  std::string get_value_text() const;

  bool get_switch_state() const;
  float get_number_value() const;

#ifdef USE_SELECT
  bool next_option();
  bool prev_option();
#endif

#ifdef USE_NUMBER
  bool inc_number();
  bool dec_number();
#endif

#ifdef USE_SWITCH
  bool toggle_switch();
#endif

  void on_enter();
  void on_leave();
  void on_value();

 protected:
  MenuItemType item_type_;
  MenuItem *parent_{nullptr};
  TemplatableValue<std::string, const MenuItem *> text_;
  std::vector<MenuItem *> items_;
  bool immediate_edit_{false};
#ifdef USE_SELECT
  select::Select *select_var_{nullptr};
#endif
#ifdef USE_NUMBER
  number::Number *number_var_{nullptr};
  std::string format_;
#endif
#ifdef USE_SWITCH
  switch_::Switch *switch_var_{nullptr};
  std::string switch_on_text_;
  std::string switch_off_text_;
#endif
  CallbackManager<void()> on_enter_callbacks_{};
  CallbackManager<void()> on_leave_callbacks_{};
  CallbackManager<void()> on_value_callbacks_{};
};

/** Class to display a hierarchical menu.
 *
 */
class DisplayMenuComponent : public Component {
 public:
  void set_root_item(MenuItem *item) { this->displayed_item_ = this->root_item_ = item; }
  void set_active(bool active) { this->active_ = active; }
  void set_rows(uint8_t rows) { this->rows_ = rows; }

  void setup() override;
  float get_setup_priority() const override { return setup_priority::PROCESSOR; }

  void up();
  void down();
  void enter();

  void show_main();
  void show();
  void hide();

  void draw();

  bool is_active() const { return this->active_; }

 protected:
  void reset_();
  void process_initial_();
  MenuItem *get_selected_item_() { return this->displayed_item_->get_item(this->cursor_index_); }
  void finish_editing_();
  virtual void draw_menu();
  virtual void draw_item(const MenuItem *item, uint8_t row, bool selected) = 0;
  virtual void update() {}
  virtual void draw_and_update() {
    draw_menu();
    update();
  }

  uint8_t rows_;
  bool active_;
  MenuItem *root_item_{nullptr};

  MenuItem *displayed_item_{nullptr};
  uint8_t top_index_{0};
  uint8_t cursor_index_{0};
  std::forward_list<std::pair<uint8_t, uint8_t>> selection_stack_{};
  bool editing_{false};
  bool root_on_enter_called_{false};
};

}  // namespace display_menu_base
}  // namespace esphome
