#include "graphical_display_menu.h"
#include "esphome/core/log.h"
#include <cstdlib>

namespace esphome {
namespace graphical_display_menu {

static const char *const TAG = "graphical_display_menu";

void GraphicalDisplayMenu::setup() {
  display::display_writer_t writer = [this](display::DisplayBuffer &it) { this->draw(); };

  display::DisplayPage *page = new display::DisplayPage(writer);
  this->display_buffer_->show_page(page);

  this->display_buffer_->clear();
  this->display_updater_->update();

  display_menu_base::DisplayMenuComponent::setup();
}

void GraphicalDisplayMenu::dump_config() {
  ESP_LOGCONFIG(TAG, "Graphical Display Menu");
  ESP_LOGCONFIG(TAG, "Has Display Buffer: %s", YESNO(this->display_buffer_ != nullptr));
  ESP_LOGCONFIG(TAG, "Has Display Updater: %s", YESNO(this->display_updater_ != nullptr));
  ESP_LOGCONFIG(TAG, "Has Font: %s", YESNO(this->font_ != nullptr));
  ESP_LOGCONFIG(TAG, "Mode: %s", this->mode_ == display_menu_base::MENU_MODE_ROTARY ? "Rotary" : "Joystick");
  ESP_LOGCONFIG(TAG, "Menu items:");
  for (size_t i = 0; i < this->displayed_item_->items_size(); i++) {
    auto *item = this->displayed_item_->get_item(i);
    ESP_LOGCONFIG(TAG, "  %i: %s (Type: %s, Immediate Edit: %s)", i, item->get_text().c_str(),
                  LOG_STR_ARG(display_menu_base::menu_item_type_to_string(item->get_type())),
                  YESNO(item->get_immediate_edit()));
  }
}

void GraphicalDisplayMenu::set_display_buffer(display::DisplayBuffer *display) { this->display_buffer_ = display; }

void GraphicalDisplayMenu::set_display_updater(PollingComponent *display_updater) {
  this->display_updater_ = display_updater;
}

void GraphicalDisplayMenu::set_font(display::Font *font) { this->font_ = font; }

void GraphicalDisplayMenu::draw_menu() {
  const int available_height = this->display_buffer_->get_height();
  int total_height = 0;
  int y_padding = 2;
  bool scroll_menu_items = false;
  std::vector<Dimension> menu_dimensions;

  for (size_t i = 0; i < this->displayed_item_->items_size(); i++) {
    auto *item = this->displayed_item_->get_item(i);
    bool selected = i == this->cursor_index_;
    Dimension d = this->measure_item(item, selected);

    menu_dimensions.push_back(d);
    total_height += d.height + y_padding;

    // Scroll the display if the selected item or the item immediately after it overflows
    if (((selected) || (i == this->cursor_index_ + 1)) && (total_height > available_height)) {
      scroll_menu_items = true;
    }
  }

  int y_offset = 0;
  int first_item_index = 0;
  int last_item_index = this->displayed_item_->items_size();

  if (scroll_menu_items) {
    // Attempt to draw the item after the current item (+1 for equality check in the draw loop)
    last_item_index = std::min(this->cursor_index_ + 2, last_item_index);

    // Go back through the measurements to determine how many prior items we can fit
    int height_left_to_use = available_height;
    for (int i = last_item_index - 1; i >= 0; i--) {
      Dimension d = menu_dimensions[i];
      height_left_to_use -= d.height - y_padding;

      if (height_left_to_use <= 0) {
        // Ran out of space -  this is our first item to draw
        first_item_index = i + 1;
        y_offset = height_left_to_use;
        break;
      }
    }
  }

  // Render the items into the view port
  for (size_t i = first_item_index; i < last_item_index; i++) {
    auto *item = this->displayed_item_->get_item(i);
    bool selected = i == this->cursor_index_;
    Dimension dimensions = menu_dimensions.at(i);

    Position position;
    position.y = y_offset;
    position.x = 0;
    this->draw_item(item, &position, &dimensions, selected);

    y_offset = position.y + dimensions.height + y_padding;
  }
}

Dimension GraphicalDisplayMenu::measure_item(const display_menu_base::MenuItem *item, bool selected) {
  Dimension dimensions;
  dimensions.width = 0;
  dimensions.height = 0;

  if (selected) {
    // TODO: Support selection glyph
    dimensions.width += 0;
    dimensions.height += 0;
  }

  std::string label = item->get_text();
  if (item->has_value()) {
    // Append to label
    label.append(" [");
    label.append(item->get_value_text());
    label.append("]");
  }

  int x1;
  int y1;
  int width;
  int height;
  this->display_buffer_->get_text_bounds(0, 0, label.c_str(), this->font_, display::TextAlign::TOP_LEFT, &x1, &y1,
                                         &width, &height);

  dimensions.width += width;
  dimensions.height = std::max(dimensions.height, height);

  return dimensions;
}

void GraphicalDisplayMenu::draw_item(const display_menu_base::MenuItem *item, const Position *position,
                                     const Dimension *measured_dimensions, bool selected) {
  auto background_color = selected ? display::COLOR_ON : display::COLOR_OFF;
  auto foreground_color = selected ? display::COLOR_OFF : display::COLOR_ON;

  int background_width = std::max(measured_dimensions->width, this->display_buffer_->get_width());

  this->display_buffer_->filled_rectangle(position->x, position->y, background_width, measured_dimensions->height,
                                          background_color);

  std::string label = item->get_text();
  if (item->has_value()) {
    // Append to label
    label.append(" [");
    label.append(item->get_value_text());
    label.append("]");
  }

  this->display_buffer_->print(position->x, position->y, this->font_, foreground_color, display::TextAlign::TOP_LEFT,
                               label.c_str());
}

void GraphicalDisplayMenu::draw_item(const display_menu_base::MenuItem *item, uint8_t row, bool selected) {
  ESP_LOGE(TAG, "draw_item(MenuItem *item, uint8_t row, bool selected) called. The graphical_display_menu specific "
                "draw_item should be called.");
}

void GraphicalDisplayMenu::update() {
  if (this->display_updater_ != nullptr) {
    this->display_updater_->update();
  }
}

}  // namespace graphical_display_menu
}  // namespace esphome
