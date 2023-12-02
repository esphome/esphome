#include "display_menu_base.h"
#include <algorithm>

namespace esphome {
namespace display_menu_base {

void DisplayMenuComponent::up() {
  if (this->check_healthy_and_active_()) {
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
  if (this->check_healthy_and_active_()) {
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
  if (this->check_healthy_and_active_()) {
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
  if (this->check_healthy_and_active_()) {
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
  if (this->check_healthy_and_active_()) {
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
  if (this->check_healthy_and_active_())
    this->draw_menu();
}

void DisplayMenuComponent::show_main() {
  bool disp_changed = false;

  if (this->is_failed())
    return;

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
  if (this->is_failed())
    return;

  this->process_initial_();

  if (!this->active_) {
    this->active_ = true;
    this->draw_and_update();
  }
}

void DisplayMenuComponent::hide() {
  if (this->check_healthy_and_active_()) {
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

bool DisplayMenuComponent::check_healthy_and_active_() {
  if (this->is_failed())
    return false;

  this->process_initial_();

  return this->active_;
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
  this->displayed_item_ = static_cast<MenuItemMenu *>(this->get_selected_item_());
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
  for (size_t i = 0; i < this->rows_ && this->top_index_ + i < this->displayed_item_->items_size(); ++i) {
    this->draw_item(this->displayed_item_->get_item(this->top_index_ + i), i,
                    this->top_index_ + i == this->cursor_index_);
  }
}

}  // namespace display_menu_base
}  // namespace esphome
