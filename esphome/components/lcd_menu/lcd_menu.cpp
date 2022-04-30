#include "lcd_menu.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <algorithm>

namespace esphome {
namespace lcd_menu {

static const char *const TAG = "lcd_character";

void LCDCharacterMenuComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "LCD Menu");
  ESP_LOGCONFIG(TAG, "  Columns: %u, Rows: %u", this->columns_, this->rows_);
  ESP_LOGCONFIG(TAG, "  Mark characters: %02x, %02x, %02x, %02x", this->mark_selected_, this->mark_editing_,
                this->mark_submenu_, this->mark_back_);
}

void LCDCharacterMenuComponent::draw_item_(const display_menu_base::MenuItem *item, uint8_t row, bool selected) {
  char data[this->columns_ + 1];  // Bounded to 65 through the config

  memset(data, ' ', this->columns_);

  if (selected)
    data[0] = this->editing_ ? this->mark_editing_ : this->mark_selected_;

  switch (item->get_type()) {
    case display_menu_base::MENU_ITEM_MENU:
      data[this->columns_ - 1] = this->mark_submenu_;
      break;
    case display_menu_base::MENU_ITEM_BACK:
      data[this->columns_ - 1] = this->mark_back_;
      break;
    default:
      break;
  }

  if (item->get_writer().has_value()) {
    auto s = item->get_writer().value()(item);
    size_t n = std::min(s.size(), (size_t) this->columns_ - 2);
    memcpy(data + 1, s.c_str(), n);
  } else {
    size_t n = std::min(item->get_text().size(), (size_t) this->columns_ - 2);
    memcpy(data + 1, item->get_text().c_str(), n);

    switch (item->get_type()) {
      case display_menu_base::MENU_ITEM_SELECT:
      case display_menu_base::MENU_ITEM_NUMBER:
      case display_menu_base::MENU_ITEM_SWITCH: {
        std::string val;

        switch (item->get_type()) {
          case display_menu_base::MENU_ITEM_SELECT:
            val = item->get_option_text();
            break;
          case display_menu_base::MENU_ITEM_NUMBER:
            val = item->get_number_text();
            break;
          case display_menu_base::MENU_ITEM_SWITCH:
            val = item->get_switch_text();
            break;
          default:
            break;
        }

        // Maximum: start mark, at least two chars of label, space, '[', value, ']',
        // end mark. Config guarantees columns >= 12
        size_t val_width = std::min((size_t) this->columns_ - 7, val.length());
        memcpy(data + this->columns_ - val_width - 4, " [", 2);
        memcpy(data + this->columns_ - val_width - 2, val.c_str(), val_width);
        data[this->columns_ - 2] = ']';
      } break;
      default:
        break;
    }
  }

  data[this->columns_] = '\0';

  this->display_->print(0, row, data);
}

}  // namespace lcd_menu
}  // namespace esphome
