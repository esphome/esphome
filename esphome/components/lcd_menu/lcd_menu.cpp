#include "lcd_menu.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <algorithm>

namespace esphome {
namespace lcd_menu {

static const char *const TAG = "lcd_menu";

void LCDMenuComponent::setup() {}

void LCDMenuComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "LCD Menu");
  ESP_LOGCONFIG(TAG, "  Columns: %u, Rows: %u", this->columns_, this->rows_);
  ESP_LOGCONFIG(TAG, "  Mark characters: %02x, %02x, %02x, %02x", this->mark_selected_, this->mark_editing_,
                this->mark_submenu_, this->mark_back_);
}

void LCDMenuComponent::up() {
  this->process_initial_();

  if (this->active_) {
    bool chg = false;

    if (this->editing_) {
      switch (this->get_selected_item_()->get_type()) {
        case MENU_ITEM_ENUM:
          chg = this->get_selected_item_()->dec_enum();
          break;
        case MENU_ITEM_NUMBER:
          chg = this->get_selected_item_()->dec_number();
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
      this->draw_and_update_();
  }
}

void LCDMenuComponent::down() {
  this->process_initial_();

  if (this->active_) {
    bool chg = false;

    if (this->editing_) {
      switch (this->get_selected_item_()->get_type()) {
        case MENU_ITEM_ENUM:
          chg = this->get_selected_item_()->inc_enum();
          break;
        case MENU_ITEM_NUMBER:
          chg = this->get_selected_item_()->inc_number();
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
      this->draw_and_update_();
  }
}

void LCDMenuComponent::enter() {
  this->process_initial_();

  if (this->active_) {
    bool chg = false;
    MenuItem *item = this->get_selected_item_();

    if (this->editing_) {
      this->finish_editing_();
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
        case MENU_ITEM_ENUM:
          if (item->get_immediate_edit()) {
            chg = item->inc_enum();
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
        case MENU_ITEM_COMMAND:
          item->on_value();
          chg = true;
          break;
        default:
          break;
      }
    }

    if (chg)
      this->draw_and_update_();
  }
}

void LCDMenuComponent::draw() {
  this->process_initial_();

  if (this->active_) {
    for (size_t i = 0; i < this->rows_ && this->top_index_ + i < this->displayed_item_->items_size(); ++i) {
      this->draw_item_(this->displayed_item_->get_item(this->top_index_ + i), i,
                       this->top_index_ + i == this->cursor_index_);
    }
  }
}

void LCDMenuComponent::show_main() {
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

  this->draw_and_update_();
}

void LCDMenuComponent::show() {
  this->process_initial_();

  if (!this->active_) {
    this->active_ = true;
    this->draw_and_update_();
  }
}

void LCDMenuComponent::hide() {
  this->process_initial_();

  if (this->active_) {
    if (this->editing_)
      this->finish_editing_();
    this->active_ = false;
    this->display_->update();
  }
}

void LCDMenuComponent::reset_() {
  this->displayed_item_ = this->root_item_;
  this->cursor_index_ = this->top_index_ = 0;
  this->selection_stack_.clear();
}

void LCDMenuComponent::process_initial_() {
  if (!this->root_on_enter_called_) {
    this->root_item_->on_enter();
    this->root_on_enter_called_ = true;
  }
}

void LCDMenuComponent::draw_item_(const MenuItem *item, uint8_t row, bool selected) {
  char data[this->columns_ + 1];  // Bounded to 65 through the config

  memset(data, ' ', this->columns_);

  if (selected)
    data[0] = this->editing_ ? this->mark_editing_ : this->mark_selected_;

  size_t n = std::min(item->get_text().size(), (size_t) this->columns_ - 2);
  memcpy(data + 1, item->get_text().c_str(), n);

  switch (item->get_type()) {
    case MENU_ITEM_MENU:
      data[this->columns_ - 1] = this->mark_submenu_;
      break;
    case MENU_ITEM_BACK:
      data[this->columns_ - 1] = this->mark_back_;
      break;
    case MENU_ITEM_ENUM:
    case MENU_ITEM_NUMBER: {
      // Maximum: start mark, at least two chars of label, space, '[', value, ']',
      // end mark. Config guarantees columns >= 12
      std::string val = (item->get_type() == MENU_ITEM_NUMBER) ? item->get_number_text() : item->get_enum_text();
      size_t val_width = std::min((size_t) this->columns_ - 7, val.length());
      memcpy(data + this->columns_ - val_width - 4, " [", 2);
      memcpy(data + this->columns_ - val_width - 2, val.c_str(), val_width);
      data[this->columns_ - 2] = ']';
    } break;
    default:
      data[this->columns_ - 1] = ' ';
      break;
  }

  data[this->columns_] = '\0';

  this->display_->print(0, row, data);
}

void LCDMenuComponent::finish_editing_() {
  switch (this->get_selected_item_()->get_type()) {
    case MENU_ITEM_ENUM:
    case MENU_ITEM_NUMBER:
      this->get_selected_item_()->on_leave();
      break;
    default:
      break;
  }

  this->editing_ = false;
}

void MenuItem::on_enter() {
  if (this->item_type_ == MENU_ITEM_ENUM && this->int_var_ != nullptr) {
    *this->int_var_ = std::max(0, std::min(*this->int_var_, (int) this->enum_values_.size() - 1));
  }

  this->on_enter_callbacks_.call();
}

void MenuItem::on_leave() { this->on_leave_callbacks_.call(); }

void MenuItem::on_value() { this->on_value_callbacks_.call(); }

bool MenuItem::inc_enum() {
  bool chg = false;

  if (this->item_type_ == MENU_ITEM_ENUM && this->int_var_ != nullptr) {
    if (*this->int_var_ < this->enum_values_.size() - 1) {
      ++*this->int_var_;
    } else {
      *this->int_var_ = 0;
    }
    this->on_value();
    chg = true;
  }

  return chg;
}

bool MenuItem::dec_enum() {
  bool chg = false;

  if (this->item_type_ == MENU_ITEM_ENUM && this->int_var_ != nullptr) {
    if (*this->int_var_ > 0) {
      --*this->int_var_;
    } else {
      *this->int_var_ = this->enum_values_.size() - 1;
    }
    this->on_value();
    chg = true;
  }

  return chg;
}

bool MenuItem::inc_number() {
  bool chg = false;

  if (this->item_type_ == MENU_ITEM_NUMBER && this->number_var_ != nullptr) {
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

  if (this->item_type_ == MENU_ITEM_NUMBER && this->number_var_ != nullptr) {
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

int MenuItem::get_enum_value() const {
  int val = 0;
  if (this->item_type_ == MENU_ITEM_ENUM && this->int_var_ != nullptr)
    val = std::max(0, std::min(*this->int_var_, (int) this->enum_values_.size() - 1));

  return val;
}

const std::string &MenuItem::get_enum_text() const {
  if (this->item_type_ == MENU_ITEM_ENUM) {
    return this->enum_values_[get_enum_value()];
  } else {
    static std::string empty_string;
    return empty_string;
  }
}

float MenuItem::get_number_value() const {
  float val = 0.0;

  if (this->item_type_ == MENU_ITEM_NUMBER && this->number_var_ != nullptr) {
    if (! this->number_var_->has_state() || this->number_var_->state < this->number_var_->traits.get_min_value()) {
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

}  // namespace lcd_menu
}  // namespace esphome
