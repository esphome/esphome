#pragma once

#include "esphome/core/defines.h"
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

#include <vector>

namespace esphome {
namespace display_menu_base {

enum MenuItemType {
  MENU_ITEM_LABEL,
  MENU_ITEM_MENU,
  MENU_ITEM_BACK,
  MENU_ITEM_SELECT,
  MENU_ITEM_NUMBER,
  MENU_ITEM_SWITCH,
  MENU_ITEM_COMMAND,
  MENU_ITEM_CUSTOM,
};

class MenuItem;
using value_getter_t = std::function<std::string(const MenuItem *)>;

class MenuItem {
 public:
  explicit MenuItem(MenuItemType t) : item_type_(t) {}
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
  void add_on_next_callback(std::function<void()> &&cb) { this->on_next_callbacks_.add(std::move(cb)); }
  void add_on_prev_callback(std::function<void()> &&cb) { this->on_prev_callbacks_.add(std::move(cb)); }
  void set_value_lambda(value_getter_t &&getter) { this->value_getter_ = getter; }

  size_t items_size() const { return this->items_.size(); }
  MenuItem *get_item(size_t i) { return this->items_[i]; }

  bool get_immediate_edit() const { return this->immediate_edit_; }

  std::string get_text() const { return const_cast<MenuItem *>(this)->text_.value(this); }
  bool has_value() const;
  std::string get_value_text() const;

  bool get_switch_state() const;
  float get_number_value() const;

  bool select_next();
  bool select_prev();

  void on_enter();
  void on_leave();

 protected:
#ifdef USE_SELECT
  bool next_option_();
  bool prev_option_();
#endif

#ifdef USE_NUMBER
  bool inc_number_();
  bool dec_number_();
#endif

#ifdef USE_SWITCH
  bool toggle_switch_();
#endif

  void on_value_();
  void on_next_();
  void on_prev_();

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
  optional<value_getter_t> value_getter_{};
  CallbackManager<void()> on_enter_callbacks_{};
  CallbackManager<void()> on_leave_callbacks_{};
  CallbackManager<void()> on_value_callbacks_{};
  CallbackManager<void()> on_next_callbacks_{};
  CallbackManager<void()> on_prev_callbacks_{};
};

}  // namespace display_menu_base
}  // namespace esphome
