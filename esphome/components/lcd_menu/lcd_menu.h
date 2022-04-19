#pragma once

#include "esphome/components/globals/globals_component.h"
#include "esphome/components/lcd_base/lcd_display.h"
#include "esphome/core/component.h"

#include <forward_list>
#include <vector>

namespace esphome {
namespace lcd_menu {

enum MenuItemType {
  MENU_ITEM_ROOT,
  MENU_ITEM_LABEL,
  MENU_ITEM_MENU,
  MENU_ITEM_BACK,
  MENU_ITEM_ENUM,
  MENU_ITEM_NUMBER,
  MENU_ITEM_COMMAND,
};

class LCDMenuOnEnterTrigger;
class LCDMenuOnLeaveTrigger;
class LCDMenuOnValueTrigger;

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
  void set_enum_values(const std::vector<std::string> &values) {
    this->enum_values_ = values;
  }
  void set_enum_variable(globals::GlobalsComponent<int> *var) {
    this->int_var_ = &var->value();
  }
  void set_enum_variable(globals::RestoringGlobalsComponent<int> *var) {
    this->int_var_ = &var->value();
  }
  void set_number_variable(globals::GlobalsComponent<float> *var) {
    this->float_var_ = &var->value();
  }
  void set_number_variable(globals::RestoringGlobalsComponent<float> *var) {
    this->float_var_ = &var->value();
  }
  void set_number_range(float min, float max, float step) {
    this->min_value_ = min;
    this->max_value_ = max;
    this->step_ = step;
  }
  void set_format(const std::string &fmt) { this->format_ = fmt; }
  void set_immediate_edit(bool val) { this->immediate_edit_ = val; }
  void add_on_enter_callback(std::function<void()> &&cb) {
    this->on_enter_callbacks_.add(std::move(cb));
  }
  void add_on_leave_callback(std::function<void()> &&cb) {
    this->on_leave_callbacks_.add(std::move(cb));
  }
  void add_on_value_callback(std::function<void()> &&cb) {
    this->on_value_callbacks_.add(std::move(cb));
  }

  size_t items_size() const { return this->items_.size(); }
  MenuItem *get_item(size_t i) { return this->items_[i]; }
  void set_text(const std::string &t) { this->text_ = t; }
  const std::string &get_text() const { return this->text_; }

  int get_enum_value() const;
  const std::string &get_enum_text() const;
  bool get_immediate_edit() const { return this->immediate_edit_; }

  float get_number_value() const;
  std::string get_number_text() const;

  void inc_enum() const;
  void dec_enum() const;

  void inc_number() const;
  void dec_number() const;

  void on_enter();
  void on_leave();
  void on_value();

protected:
  MenuItemType item_type_;
  MenuItem *parent_{nullptr};
  std::string text_;
  std::vector<MenuItem *> items_;
  std::vector<std::string> enum_values_;
  bool immediate_edit_{false};
  float min_value_, max_value_, step_;
  std::string format_;
  int *int_var_{nullptr};
  float *float_var_{nullptr};
  CallbackManager<void()> on_enter_callbacks_{};
  CallbackManager<void()> on_leave_callbacks_{};
  CallbackManager<void()> on_value_callbacks_{};
};

/** Class to display a hierarchical menu.
 *
 */
class LCDMenuComponent : public Component {
public:
  void set_display(lcd_base::LCDDisplay *display) { this->display_ = display; }
  void set_dimensions(uint8_t columns, uint8_t rows) {
    this->columns_ = columns;
    this->rows_ = rows;
  }
  void set_root_item(MenuItem *item) {
    this->displayed_item_ = this->root_item_ = item;
  }
  void set_active(bool active) { this->active_ = active; }
  void set_mark_selected(uint8_t c) { this->mark_selected_ = c; }
  void set_mark_editing(uint8_t c) { this->mark_editing_ = c; }
  void set_mark_submenu(uint8_t c) { this->mark_submenu_ = c; }
  void set_mark_back(uint8_t c) { this->mark_back_ = c; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override {
    return setup_priority::PROCESSOR;
  }

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
  MenuItem *get_selected_item_() {
    return this->displayed_item_->get_item(this->cursor_index_);
  }
  void finish_editing_();
  void draw_item_(const MenuItem *item, uint8_t row, bool selected);
  void draw_and_update_() {
    draw();
    this->display_->update();
  }

  lcd_base::LCDDisplay *display_;
  uint8_t columns_;
  uint8_t rows_;
  bool active_;
  char mark_selected_;
  char mark_editing_;
  char mark_submenu_;
  char mark_back_;
  MenuItem *root_item_{nullptr};

  MenuItem *displayed_item_{nullptr};
  uint8_t top_index_{0};
  uint8_t cursor_index_{0};
  std::forward_list<std::pair<uint8_t, uint8_t>> selection_stack_{};
  bool editing_{false};
  bool root_on_enter_called_{false};
};

} // namespace lcd_menu
} // namespace esphome
