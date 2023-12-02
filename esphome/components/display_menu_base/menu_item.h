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
class MenuItemMenu;
using value_getter_t = std::function<std::string(const MenuItem *)>;

class MenuItem {
 public:
  explicit MenuItem(MenuItemType t) : item_type_(t) {}
  void set_parent(MenuItemMenu *parent) { this->parent_ = parent; }
  MenuItemMenu *get_parent() { return this->parent_; }
  MenuItemType get_type() const { return this->item_type_; }
  template<typename V> void set_text(V val) { this->text_ = val; }
  void add_on_enter_callback(std::function<void()> &&cb) { this->on_enter_callbacks_.add(std::move(cb)); }
  void add_on_leave_callback(std::function<void()> &&cb) { this->on_leave_callbacks_.add(std::move(cb)); }
  void add_on_value_callback(std::function<void()> &&cb) { this->on_value_callbacks_.add(std::move(cb)); }

  std::string get_text() const { return const_cast<MenuItem *>(this)->text_.value(this); }
  virtual bool get_immediate_edit() const { return false; }
  virtual bool has_value() const { return false; }
  virtual std::string get_value_text() const { return ""; }

  virtual bool select_next() { return false; }
  virtual bool select_prev() { return false; }

  void on_enter();
  void on_leave();

 protected:
  void on_value_();

  MenuItemType item_type_;
  MenuItemMenu *parent_{nullptr};
  TemplatableValue<std::string, const MenuItem *> text_;

  CallbackManager<void()> on_enter_callbacks_{};
  CallbackManager<void()> on_leave_callbacks_{};
  CallbackManager<void()> on_value_callbacks_{};
};

class MenuItemMenu : public MenuItem {
 public:
  explicit MenuItemMenu() : MenuItem(MENU_ITEM_MENU) {}
  void add_item(MenuItem *item) {
    item->set_parent(this);
    this->items_.push_back(item);
  }
  size_t items_size() const { return this->items_.size(); }
  MenuItem *get_item(size_t i) { return this->items_[i]; }

 protected:
  std::vector<MenuItem *> items_;
};

class MenuItemEditable : public MenuItem {
 public:
  explicit MenuItemEditable(MenuItemType t) : MenuItem(t) {}
  void set_immediate_edit(bool val) { this->immediate_edit_ = val; }
  bool get_immediate_edit() const override { return this->immediate_edit_; }
  void set_value_lambda(value_getter_t &&getter) { this->value_getter_ = getter; }

 protected:
  bool immediate_edit_{false};
  optional<value_getter_t> value_getter_{};
};

#ifdef USE_SELECT
class MenuItemSelect : public MenuItemEditable {
 public:
  explicit MenuItemSelect() : MenuItemEditable(MENU_ITEM_SELECT) {}
  void set_select_variable(select::Select *var) { this->select_var_ = var; }

  bool has_value() const override { return true; }
  std::string get_value_text() const override;

  bool select_next() override;
  bool select_prev() override;

 protected:
  select::Select *select_var_{nullptr};
};
#endif

#ifdef USE_NUMBER
class MenuItemNumber : public MenuItemEditable {
 public:
  explicit MenuItemNumber() : MenuItemEditable(MENU_ITEM_NUMBER) {}
  void set_number_variable(number::Number *var) { this->number_var_ = var; }
  void set_format(const std::string &fmt) { this->format_ = fmt; }

  bool has_value() const override { return true; }
  std::string get_value_text() const override;

  bool select_next() override;
  bool select_prev() override;

 protected:
  float get_number_value_() const;

  number::Number *number_var_{nullptr};
  std::string format_;
};
#endif

#ifdef USE_SWITCH
class MenuItemSwitch : public MenuItemEditable {
 public:
  explicit MenuItemSwitch() : MenuItemEditable(MENU_ITEM_SWITCH) {}
  void set_switch_variable(switch_::Switch *var) { this->switch_var_ = var; }
  void set_on_text(const std::string &t) { this->switch_on_text_ = t; }
  void set_off_text(const std::string &t) { this->switch_off_text_ = t; }

  bool has_value() const override { return true; }
  std::string get_value_text() const override;

  bool select_next() override;
  bool select_prev() override;

 protected:
  bool get_switch_state_() const;
  bool toggle_switch_();

  switch_::Switch *switch_var_{nullptr};
  std::string switch_on_text_;
  std::string switch_off_text_;
};
#endif

class MenuItemCommand : public MenuItem {
 public:
  explicit MenuItemCommand() : MenuItem(MENU_ITEM_COMMAND) {}

  bool select_next() override;
  bool select_prev() override;
};

class MenuItemCustom : public MenuItemEditable {
 public:
  explicit MenuItemCustom() : MenuItemEditable(MENU_ITEM_CUSTOM) {}
  void add_on_next_callback(std::function<void()> &&cb) { this->on_next_callbacks_.add(std::move(cb)); }
  void add_on_prev_callback(std::function<void()> &&cb) { this->on_prev_callbacks_.add(std::move(cb)); }

  bool has_value() const override { return this->value_getter_.has_value(); }
  std::string get_value_text() const override;

  bool select_next() override;
  bool select_prev() override;

 protected:
  void on_next_();
  void on_prev_();

  CallbackManager<void()> on_next_callbacks_{};
  CallbackManager<void()> on_prev_callbacks_{};
};

}  // namespace display_menu_base
}  // namespace esphome
