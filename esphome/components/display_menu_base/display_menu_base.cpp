#include "display_menu_base.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <algorithm>

namespace esphome {
namespace display_menu_base {

static const char *const TAG = "display_menu";

void DisplayMenuComponent::setup() {}

void DisplayMenuComponent::up() {
  this->process_initial_();

  if (this->active_) {
    bool chg = false;

    if (this->editing_) {
      switch (this->get_selected_item_()->get_type()) {
        case MENU_ITEM_SELECT:
          chg = this->get_selected_item_()->prev_option();
          break;
        case MENU_ITEM_NUMBER:
          chg = this->get_selected_item_()->dec_number();
          break;
        case MENU_ITEM_SWITCH:
          chg = this->get_selected_item_()->toggle_switch();
          break;
        default:
          break;
      }
    } else {
      if (this->cursor_index_ > 0) {
        chg = true;

        --this->cursor_index_;

        if (this->cursor_index_ < this->top_index_)
          this->top_index_ = this->cursor_index_;
      }
    }

    if (chg)
      this->draw_and_update();
  }
}

void DisplayMenuComponent::down() {
  this->process_initial_();

  if (this->active_) {
    bool chg = false;

    if (this->editing_) {
      switch (this->get_selected_item_()->get_type()) {
        case MENU_ITEM_SELECT:
          chg = this->get_selected_item_()->next_option();
          break;
        case MENU_ITEM_NUMBER:
          chg = this->get_selected_item_()->inc_number();
          break;
        case MENU_ITEM_SWITCH:
          chg = this->get_selected_item_()->toggle_switch();
          break;
        default:
          break;
      }
    } else {
      if (this->cursor_index_ + 1 < this->displayed_item_->items_size()) {
        chg = true;

        ++this->cursor_index_;

        if (this->cursor_index_ >= this->top_index_ + this->rows_)
          this->top_index_ = this->cursor_index_ - this->rows_ + 1;
      }
    }

    if (chg)
      this->draw_and_update();
  }
}

void DisplayMenuComponent::enter() {
  this->process_initial_();

  if (this->active_) {
    bool chg = false;
    MenuItem *item = this->get_selected_item_();

    if (this->editing_) {
      this->finish_editing_();
      chg = true;
    } else {
      switch (item->get_type()) {
        case MENU_ITEM_MENU:
          this->displayed_item_->on_leave();
          this->displayed_item_ = this->get_selected_item_();
          this->selection_stack_.push_front({this->top_index_, this->cursor_index_});
          this->cursor_index_ = this->top_index_ = 0;
          this->displayed_item_->on_enter();
          chg = true;
          break;
        case MENU_ITEM_BACK:
          if (this->displayed_item_->get_parent() != nullptr) {
            this->displayed_item_->on_leave();
            this->displayed_item_ = this->displayed_item_->get_parent();
            this->top_index_ = this->selection_stack_.front().first;
            this->cursor_index_ = this->selection_stack_.front().second;
            this->selection_stack_.pop_front();
            this->displayed_item_->on_enter();
            chg = true;
          }
          break;
        case MENU_ITEM_SELECT:
          if (item->get_immediate_edit()) {
            chg = item->next_option();
          } else {
            this->editing_ = true;
            item->on_enter();
            chg = true;
          }
          break;
        case MENU_ITEM_NUMBER:
          this->editing_ = true;
          item->on_enter();
          chg = true;
          break;
        case MENU_ITEM_SWITCH:
          if (item->get_immediate_edit()) {
            chg = item->toggle_switch();
          } else {
            this->editing_ = true;
            item->on_enter();
            chg = true;
          }
          break;
        case MENU_ITEM_COMMAND:
          item->on_value();
          chg = true;
          break;
        default:
          break;
      }
    }

    if (chg)
      this->draw_and_update();
  }
}

void DisplayMenuComponent::draw() {
  this->process_initial_();
  this->draw_menu();
}

void DisplayMenuComponent::show_main() {
  bool disp_changed = false;

  this->process_initial_();

  if (this->active_ && this->editing_)
    this->finish_editing_();

  if (this->displayed_item_ != this->root_item_) {
    this->displayed_item_->on_leave();
    disp_changed = true;
  }

  this->reset_();
  this->active_ = true;

  if (disp_changed) {
    this->displayed_item_->on_enter();
  }

  this->draw_and_update();
}

void DisplayMenuComponent::show() {
  this->process_initial_();

  if (!this->active_) {
    this->active_ = true;
    this->draw_and_update();
  }
}

void DisplayMenuComponent::hide() {
  this->process_initial_();

  if (this->active_) {
    if (this->editing_)
      this->finish_editing_();
    this->active_ = false;
    this->update();
  }
}

void DisplayMenuComponent::reset_() {
  this->displayed_item_ = this->root_item_;
  this->cursor_index_ = this->top_index_ = 0;
  this->selection_stack_.clear();
}

void DisplayMenuComponent::process_initial_() {
  if (!this->root_on_enter_called_) {
    this->root_item_->on_enter();
    this->root_on_enter_called_ = true;
  }
}

void DisplayMenuComponent::finish_editing_() {
  switch (this->get_selected_item_()->get_type()) {
    case MENU_ITEM_SELECT:
    case MENU_ITEM_NUMBER:
    case MENU_ITEM_SWITCH:
      this->get_selected_item_()->on_leave();
      break;
    default:
      break;
  }

  this->editing_ = false;
}

void DisplayMenuComponent::draw_menu() {
  if (this->active_) {
    for (size_t i = 0; i < this->rows_ && this->top_index_ + i < this->displayed_item_->items_size(); ++i) {
      this->draw_item(this->displayed_item_->get_item(this->top_index_ + i), i,
                       this->top_index_ + i == this->cursor_index_);
    }
  }
}

void MenuItem::on_enter() { this->on_enter_callbacks_.call(); }

void MenuItem::on_leave() { this->on_leave_callbacks_.call(); }

void MenuItem::on_value() { this->on_value_callbacks_.call(); }

bool MenuItem::next_option() {
  bool chg = false;

  if (this->select_var_ != nullptr) {
    auto options = this->select_var_->traits.get_options();

    if (!options.empty()) {
      auto opt = std::find(options.begin(), options.end(), this->select_var_->state);

      if (opt != options.end()) {
        ++opt;
      }

      if (opt == options.end()) {
        opt = options.begin();
      }

      this->select_var_->set(*opt);
      this->on_value();
      chg = true;
    }
  }

  return chg;
}

bool MenuItem::prev_option() {
  bool chg = false;

  if (this->select_var_ != nullptr) {
    auto options = this->select_var_->traits.get_options();

    if (!options.empty()) {
      auto opt = std::find(options.begin(), options.end(), this->select_var_->state);

      if (opt == options.begin()) {
        opt = options.end();
      }

      --opt;

      this->select_var_->set(*opt);
      this->on_value();
      chg = true;
    }
  }

  return chg;
}

bool MenuItem::inc_number() {
  bool chg = false;

  if (this->number_var_ != nullptr) {
    float val = this->get_number_value() + this->number_var_->traits.get_step();
    if (val > this->number_var_->traits.get_max_value()) {
      val = this->number_var_->traits.get_max_value();
    }

    if (val != this->number_var_->state) {
      this->number_var_->set(val);
      this->on_value();
      chg = true;
    }
  }

  return chg;
}

bool MenuItem::dec_number() {
  bool chg = false;

  if (this->number_var_ != nullptr) {
    float val = this->get_number_value() - this->number_var_->traits.get_step();
    if (val < this->number_var_->traits.get_min_value()) {
      val = this->number_var_->traits.get_min_value();
    }

    if (val != this->number_var_->state) {
      this->number_var_->set(val);
      this->on_value();
      chg = true;
    }
  }

  return chg;
}

bool MenuItem::toggle_switch() {
  bool chg = false;

  if (this->switch_var_ != nullptr) {
    this->switch_var_->toggle();
    this->on_value();
    chg = true;
  }

  return chg;
}

const std::string &MenuItem::get_option_text() const {
  if (this->item_type_ == MENU_ITEM_SELECT && this->select_var_ != nullptr) {
    return this->select_var_->state;
  } else {
    static std::string empty_string;
    return empty_string;
  }
}

float MenuItem::get_number_value() const {
  float val = 0.0;

  if (this->item_type_ == MENU_ITEM_NUMBER && this->number_var_ != nullptr) {
    if (!this->number_var_->has_state() || this->number_var_->state < this->number_var_->traits.get_min_value()) {
      val = this->number_var_->traits.get_min_value();
    } else if (this->number_var_->state > this->number_var_->traits.get_max_value()) {
      val = this->number_var_->traits.get_max_value();
    } else {
      val = this->number_var_->state;
    }
  }

  return val;
}

std::string MenuItem::get_number_text() const {
  char data[32];
  snprintf(data, sizeof(data), this->format_.c_str(), get_number_value());
  return data;
}

bool MenuItem::get_switch_state() const {
  return (this->item_type_ == MENU_ITEM_SWITCH && this->switch_var_ != nullptr && this->switch_var_->state);
}

const std::string &MenuItem::get_switch_text() const {
  return this->get_switch_state() ? this->switch_on_text_ : this->switch_off_text_;
}

}  // namespace display_menu_base
}  // namespace esphome
