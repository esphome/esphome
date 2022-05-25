#include "menu_item.h"

#include <cstdio>

namespace esphome {
namespace display_menu_base {

bool MenuItem::select_next() {
  bool result;

  switch (this->item_type_) {
#ifdef USE_SELECT
    case MENU_ITEM_SELECT:
      result = this->next_option_();
      break;
#endif
#ifdef USE_NUMBER
    case MENU_ITEM_NUMBER:
      result = this->inc_number_();
      break;
#endif
#ifdef USE_SWITCH
    case MENU_ITEM_SWITCH:
      result = this->toggle_switch_();
      break;
#endif
    case MENU_ITEM_COMMAND:
      this->on_value_();
      result = true;
      break;
    case MENU_ITEM_CUSTOM:
      this->on_next_();
      this->on_value_();
      result = true;
      break;
    default:
      result = false;
      break;
  }

  return result;
}

bool MenuItem::select_prev() {
  bool result;

  switch (this->item_type_) {
#ifdef USE_SELECT
    case MENU_ITEM_SELECT:
      result = this->prev_option_();
      break;
#endif
#ifdef USE_NUMBER
    case MENU_ITEM_NUMBER:
      result = this->dec_number_();
      break;
#endif
#ifdef USE_SWITCH
    case MENU_ITEM_SWITCH:
      result = this->toggle_switch_();
      break;
#endif
    case MENU_ITEM_COMMAND:
      this->on_value_();
      result = true;
      break;
    case MENU_ITEM_CUSTOM:
      this->on_prev_();
      this->on_value_();
      result = true;
      break;
    default:
      result = false;
      break;
  }

  return result;
}

void MenuItem::on_enter() { this->on_enter_callbacks_.call(); }

void MenuItem::on_leave() { this->on_leave_callbacks_.call(); }

#ifdef USE_SELECT
bool MenuItem::next_option_() {
  bool changed = false;

  if (this->select_var_ != nullptr) {
    this->select_var_->make_call().select_next(true).perform();
    changed = true;
  }

  return changed;
}

bool MenuItem::prev_option_() {
  bool changed = false;

  if (this->select_var_ != nullptr) {
    this->select_var_->make_call().select_previous(true).perform();
    changed = true;
  }

  return changed;
}
#endif  // USE_SELECT

#ifdef USE_NUMBER
bool MenuItem::inc_number_() {
  bool changed = false;

  if (this->number_var_ != nullptr) {
    float last = this->number_var_->state;
    this->number_var_->make_call().number_increment(false).perform();

    if (this->number_var_->state != last) {
      this->on_value_();
      changed = true;
    }
  }

  return changed;
}

bool MenuItem::dec_number_() {
  bool changed = false;

  if (this->number_var_ != nullptr) {
    float last = this->number_var_->state;
    this->number_var_->make_call().number_decrement(false).perform();

    if (this->number_var_->state != last) {
      this->on_value_();
      changed = true;
    }
  }

  return changed;
}
#endif  // USE_NUMBER

#ifdef USE_SWITCH
bool MenuItem::toggle_switch_() {
  bool changed = false;

  if (this->switch_var_ != nullptr) {
    this->switch_var_->toggle();
    this->on_value_();
    changed = true;
  }

  return changed;
}
#endif  // USE_SWITCH

bool MenuItem::has_value() const {
  bool result;

  switch (this->item_type_) {
    case MENU_ITEM_SELECT:
    case MENU_ITEM_NUMBER:
    case MENU_ITEM_SWITCH:
      result = true;
      break;
    case MENU_ITEM_CUSTOM:
      result = this->value_getter_.has_value();
      break;
    default:
      result = false;
      break;
  }

  return result;
}

std::string MenuItem::get_value_text() const {
  std::string result;

  if (this->value_getter_.has_value()) {
    result = this->value_getter_.value()(this);
  } else {
    switch (this->item_type_) {
#ifdef USE_SELECT
      case MENU_ITEM_SELECT:
        if (this->select_var_ != nullptr) {
          result = this->select_var_->state;
        }
        break;
#endif
#ifdef USE_NUMBER
      case MENU_ITEM_NUMBER:
        char data[32];
        snprintf(data, sizeof(data), this->format_.c_str(), get_number_value());
        result = data;
        break;
#endif
#ifdef USE_SWITCH
      case MENU_ITEM_SWITCH:
        result = this->get_switch_state() ? this->switch_on_text_ : this->switch_off_text_;
        break;
#endif
      default:
        break;
    }
  }

  return result;
}

float MenuItem::get_number_value() const {
  float result = 0.0;

#ifdef USE_NUMBER
  if (this->number_var_ != nullptr) {
    if (!this->number_var_->has_state() || this->number_var_->state < this->number_var_->traits.get_min_value()) {
      result = this->number_var_->traits.get_min_value();
    } else if (this->number_var_->state > this->number_var_->traits.get_max_value()) {
      result = this->number_var_->traits.get_max_value();
    } else {
      result = this->number_var_->state;
    }
  }
#endif

  return result;
}

bool MenuItem::get_switch_state() const {
#ifdef USE_SWITCH
  return (this->switch_var_ != nullptr && this->switch_var_->state);
#else
  return false;
#endif
}

void MenuItem::on_value_() { this->on_value_callbacks_.call(); }

void MenuItem::on_next_() { this->on_next_callbacks_.call(); }

void MenuItem::on_prev_() { this->on_prev_callbacks_.call(); }

}  // namespace display_menu_base
}  // namespace esphome
