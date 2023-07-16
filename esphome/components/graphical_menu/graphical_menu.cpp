#include "graphical_menu.h"
#include "esphome/core/log.h"
#include <algorithm>

namespace esphome {
namespace graphical_menu {

static const char *const TAG = "graphical_menu";

void GraphicalMenuComponent::setup() {
  int width;
  int x_offset;
  int x_baseline;
  int height;
  char test_string[2] = "A";
  this->font_->measure(test_string, &width, &x_offset, &x_baseline, &height);
  set_rows(this->display_->get_height() / height);
  this->columns_ = this->display_->get_width() / width;  // Assumes monospaced font
  ESP_LOGD(TAG, "Character Width: %u, Height: %u", width, height);
  display_menu_base::DisplayMenuComponent::setup();
}

float GraphicalMenuComponent::get_setup_priority() const { return setup_priority::PROCESSOR - 1.0f; }

void GraphicalMenuComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "LCD Menu");
  ESP_LOGCONFIG(TAG, "  Rows: %u Colums: %u", this->rows_, this->columns_);
  ESP_LOGCONFIG(TAG, "  Mark characters: %02x, %02x, %02x, %02x", this->mark_selected_, this->mark_editing_,
                this->mark_submenu_, this->mark_back_);
}

void GraphicalMenuComponent::draw_item(const display_menu_base::MenuItem *item, uint8_t row, bool selected) {
  char data[this->columns_ + 1];

  int editing_width;
  int selected_width;
  int x_offset;
  int x_baseline;
  int height;
  char editing_string[2] = {this->mark_editing_, '\0'};
  char selected_string[2] = {this->mark_selected_, '\0'};
  this->font_->measure(editing_string, &editing_width, &x_offset, &x_baseline, &height);
  this->font_->measure(selected_string, &selected_width, &x_offset, &x_baseline, &height);

  int es_width = editing_width >= selected_width ? editing_width : selected_width;

  memset(data, ' ', this->columns_);

  if (selected) {
    char c = (this->editing_ || (this->mode_ == display_menu_base::MENU_MODE_JOYSTICK && item->get_immediate_edit()))
                 ? this->mark_editing_
                 : this->mark_selected_;
    this->display_->printf(0, row * this->font_->get_height(), this->font_, this->color_, "%c", c);
  }

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

  auto text = item->get_text();

  this->display_->print(es_width, row * this->font_->get_height(), this->font_, this->color_, item->get_text().c_str());

  if (item->has_value()) {
    std::string value = item->get_value_text();

    this->display_->printf(this->display_->get_width(), row * this->font_->get_height(), this->font_, this->color_,
                           display::TextAlign::TOP_RIGHT, "[%s]", value.c_str());
  }
}

}  // namespace graphical_menu
}  // namespace esphome
