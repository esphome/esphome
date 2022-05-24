#include "display_menu_base.h"
#include "esphome/core/log.h"
#include <algorithm>

namespace esphome {
namespace display_menu_base {

static const char *const TAG = "display_menu";

void DisplayMenuComponent::up() {
  this->process_initial_();

  if (this->active_) {
    bool changed = false;

    if (this->editing_) {
      switch (this->mode_) {
        case MENU_MODE_ROTARY:
          changed = this->get_selected_item_()->select_prev();
          break;
        default:
          break;
      }
    } else {
      changed = this->cursor_up_();
    }

    if (changed)
      this->draw_and_update();
  }
}

void DisplayMenuComponent::down() {
  this->process_initial_();

  if (this->active_) {
    bool changed = false;

    if (this->editing_) {
      switch (this->mode_) {
        case MENU_MODE_ROTARY:
          changed = this->get_selected_item_()->select_next();
          break;
        default:
          break;
      }
    } else {
      changed = this->cursor_down_();
    }

    if (changed)
      this->draw_and_update();
  }
}

void DisplayMenuComponent::left() {
  this->process_initial_();

  if (this->active_) {
    bool changed = false;

    switch (this->get_selected_item_()->get_type()) {
      case MENU_ITEM_SELECT:
      case MENU_ITEM_SWITCH:
      case MENU_ITEM_NUMBER:
      case MENU_ITEM_CUSTOM:
        switch (this->mode_) {
          case MENU_MODE_ROTARY:
            if (this->editing_) {
              this->finish_editing_();
              changed = true;
            }
            break;
          case MENU_MODE_JOYSTICK:
            if (this->editing_ || this->get_selected_item_()->get_immediate_edit())
              changed = this->get_selected_item_()->select_prev();
            break;
          default:
            break;
        }
        break;
      case MENU_ITEM_BACK:
        changed = this->leave_menu_();
        break;
      default:
        break;
    }

    if (changed)
      this->draw_and_update();
  }
}

void DisplayMenuComponent::right() {
  this->process_initial_();

  if (this->active_) {
    bool changed = false;

    switch (this->get_selected_item_()->get_type()) {
      case MENU_ITEM_SELECT:
      case MENU_ITEM_SWITCH:
      case MENU_ITEM_NUMBER:
      case MENU_ITEM_CUSTOM:
        switch (this->mode_) {
          case MENU_MODE_JOYSTICK:
            if (this->editing_ || this->get_selected_item_()->get_immediate_edit())
              changed = this->get_selected_item_()->select_next();
          default:
            break;
        }
        break;
      case MENU_ITEM_MENU:
        changed = this->enter_menu_();
        break;
      default:
        break;
    }

    if (changed)
      this->draw_and_update();
  }
}

void DisplayMenuComponent::enter() {
  this->process_initial_();

  if (this->active_) {
    bool changed = false;
    MenuItem *item = this->get_selected_item_();

    if (this->editing_) {
      this->finish_editing_();
      changed = true;
    } else {
      switch (item->get_type()) {
        case MENU_ITEM_MENU:
          changed = this->enter_menu_();
          break;
        case MENU_ITEM_BACK:
          changed = this->leave_menu_();
          break;
        case MENU_ITEM_SELECT:
        case MENU_ITEM_SWITCH:
        case MENU_ITEM_CUSTOM:
          if (item->get_immediate_edit()) {
            changed = item->select_next();
          } else {
            this->editing_ = true;
            item->on_enter();
            changed = true;
          }
          break;
        case MENU_ITEM_NUMBER:
          // A number cannot be immediate in the rotary mode
          if (!item->get_immediate_edit() || this->mode_ == MENU_MODE_ROTARY) {
            this->editing_ = true;
            item->on_enter();
            changed = true;
          }
          break;
        case MENU_ITEM_COMMAND:
          changed = item->select_next();
          break;
        default:
          break;
      }
    }

    if (changed)
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

bool DisplayMenuComponent::cursor_up_() {
  bool changed = false;

  if (this->cursor_index_ > 0) {
    changed = true;

    --this->cursor_index_;

    if (this->cursor_index_ < this->top_index_)
      this->top_index_ = this->cursor_index_;
  }

  return changed;
}

bool DisplayMenuComponent::cursor_down_() {
  bool changed = false;

  if (this->cursor_index_ + 1 < this->displayed_item_->items_size()) {
    changed = true;

    ++this->cursor_index_;

    if (this->cursor_index_ >= this->top_index_ + this->rows_)
      this->top_index_ = this->cursor_index_ - this->rows_ + 1;
  }

  return changed;
}

bool DisplayMenuComponent::enter_menu_() {
  this->displayed_item_->on_leave();
  this->displayed_item_ = this->get_selected_item_();
  this->selection_stack_.push_front({this->top_index_, this->cursor_index_});
  this->cursor_index_ = this->top_index_ = 0;
  this->displayed_item_->on_enter();

  return true;
}

bool DisplayMenuComponent::leave_menu_() {
  bool changed = false;

  if (this->displayed_item_->get_parent() != nullptr) {
    this->displayed_item_->on_leave();
    this->displayed_item_ = this->displayed_item_->get_parent();
    this->top_index_ = this->selection_stack_.front().first;
    this->cursor_index_ = this->selection_stack_.front().second;
    this->selection_stack_.pop_front();
    this->displayed_item_->on_enter();
    changed = true;
  }

  return changed;
}

void DisplayMenuComponent::finish_editing_() {
  switch (this->get_selected_item_()->get_type()) {
    case MENU_ITEM_SELECT:
    case MENU_ITEM_NUMBER:
    case MENU_ITEM_SWITCH:
    case MENU_ITEM_CUSTOM:
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
