#include "menu_item.h"
#include "esphome/core/progmem.h"

#include <cstdio>

namespace esphome::display_menu_base {

// Indexed by MenuItemType
PROGMEM_STRING_TABLE(MenuItemTypeStrings, "MENU_ITEM_LABEL", "MENU_ITEM_MENU", "MENU_ITEM_BACK", "MENU_ITEM_SELECT",
                     "MENU_ITEM_NUMBER", "MENU_ITEM_SWITCH", "MENU_ITEM_COMMAND", "MENU_ITEM_CUSTOM", "UNKNOWN");

const LogString *menu_item_type_to_string(MenuItemType type) {
  return MenuItemTypeStrings::get_log_str(static_cast<uint8_t>(type), MenuItemTypeStrings::LAST_INDEX);
}

void MenuItem::on_enter() { this->on_enter_callbacks_.call(); }

void MenuItem::on_leave() { this->on_leave_callbacks_.call(); }

void MenuItem::on_value_() { this->on_value_callbacks_.call(); }

#ifdef USE_SELECT
std::string MenuItemSelect::get_value_text() const {
  std::string result;

  if (this->value_getter_.has_value()) {
    result = this->value_getter_.value()(this);
  } else {
    if (this->variable_select_ != nullptr) {
      auto option = this->variable_select_->current_option();
      result.assign(option.c_str(), option.size());
    }
  }

  return result;
}

bool MenuItemSelect::select_next() {
  bool changed = false;

  if (this->variable_select_ != nullptr) {
    this->variable_select_->make_call().select_next(true).perform();
    this->on_value_();
    changed = true;
  }

  return changed;
}

bool MenuItemSelect::select_prev() {
  bool changed = false;

  if (this->variable_select_ != nullptr) {
    this->variable_select_->make_call().select_previous(true).perform();
    this->on_value_();
    changed = true;
  }

  return changed;
}
#endif  // USE_SELECT

#ifdef USE_NUMBER
std::string MenuItemNumber::get_value_text() const {
  std::string result;

  if (this->value_getter_.has_value()) {
    result = this->value_getter_.value()(this);
  } else {
    char data[32];
    snprintf(data, sizeof(data), this->format_, get_number_value_());
    result = data;
  }

  return result;
}

bool MenuItemNumber::select_next() {
  bool changed = false;

  if (this->variable_number_ != nullptr) {
    float last = this->variable_number_->state;
    this->variable_number_->make_call().number_increment(false).perform();

    if (this->variable_number_->state != last) {
      this->on_value_();
      changed = true;
    }
  }

  return changed;
}

bool MenuItemNumber::select_prev() {
  bool changed = false;

  if (this->variable_number_ != nullptr) {
    float last = this->variable_number_->state;
    this->variable_number_->make_call().number_decrement(false).perform();

    if (this->variable_number_->state != last) {
      this->on_value_();
      changed = true;
    }
  }

  return changed;
}

float MenuItemNumber::get_number_value_() const {
  float result = 0.0;

  if (this->variable_number_ != nullptr) {
    if (!this->variable_number_->has_state() ||
        this->variable_number_->state < this->variable_number_->traits.get_min_value()) {
      result = this->variable_number_->traits.get_min_value();
    } else if (this->variable_number_->state > this->variable_number_->traits.get_max_value()) {
      result = this->variable_number_->traits.get_max_value();
    } else {
      result = this->variable_number_->state;
    }
  }

  return result;
}
#endif  // USE_NUMBER

#ifdef USE_SWITCH
std::string MenuItemSwitch::get_value_text() const {
  std::string result;

  if (this->value_getter_.has_value()) {
    result = this->value_getter_.value()(this);
  } else {
    result = this->get_switch_state_() ? this->switch_on_text_ : this->switch_off_text_;
  }

  return result;
}

bool MenuItemSwitch::select_next() { return this->toggle_switch_(); }

bool MenuItemSwitch::select_prev() { return this->toggle_switch_(); }

bool MenuItemSwitch::get_switch_state_() const {
  return (this->variable_switch_ != nullptr && this->variable_switch_->state);
}

bool MenuItemSwitch::toggle_switch_() {
  bool changed = false;

  if (this->variable_switch_ != nullptr) {
    this->variable_switch_->toggle();
    this->on_value_();
    changed = true;
  }

  return changed;
}
#endif  // USE_SWITCH

std::string MenuItemCustom::get_value_text() const {
  return (this->value_getter_.has_value()) ? this->value_getter_.value()(this) : "";
}

bool MenuItemCommand::select_next() {
  this->on_value_();
  return true;
}

bool MenuItemCommand::select_prev() {
  this->on_value_();
  return true;
}

bool MenuItemCustom::select_next() {
  this->on_next_();
  this->on_value_();
  return true;
}

bool MenuItemCustom::select_prev() {
  this->on_prev_();
  this->on_value_();
  return true;
}

void MenuItemCustom::on_next_() { this->on_next_callbacks_.call(); }

void MenuItemCustom::on_prev_() { this->on_prev_callbacks_.call(); }

}  // namespace esphome::display_menu_base
