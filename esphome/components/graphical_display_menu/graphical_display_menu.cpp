#include "graphical_display_menu.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <cstdlib>
#include "esphome/components/display/display.h"

namespace esphome {
namespace graphical_display_menu {

static const char *const TAG = "graphical_display_menu";

void GraphicalDisplayMenu::setup() {
  if (this->display_ != nullptr) {
    display::display_writer_t writer = [this](display::Display &it) { this->draw_menu(); };
    this->display_page_ = make_unique<display::DisplayPage>(writer);
  }

  if (!this->menu_item_value_.has_value()) {
    this->menu_item_value_ = [](const MenuItemValueArguments *it) {
      std::string label = " ";
      if (it->is_item_selected && it->is_menu_editing) {
        label.append(">");
        label.append(it->item->get_value_text());
        label.append("<");
      } else {
        label.append("(");
        label.append(it->item->get_value_text());
        label.append(")");
      }
      return label;
    };
  }

  display_menu_base::DisplayMenuComponent::setup();
}

void GraphicalDisplayMenu::dump_config() {
  ESP_LOGCONFIG(TAG, "Graphical Display Menu");
  ESP_LOGCONFIG(TAG, "Has Display: %s", YESNO(this->display_ != nullptr));
  ESP_LOGCONFIG(TAG, "Popup Mode: %s", YESNO(this->display_ != nullptr));
  ESP_LOGCONFIG(TAG, "Advanced Drawing Mode: %s", YESNO(this->display_ == nullptr));
  ESP_LOGCONFIG(TAG, "Has Font: %s", YESNO(this->font_ != nullptr));
  ESP_LOGCONFIG(TAG, "Mode: %s", this->mode_ == display_menu_base::MENU_MODE_ROTARY ? "Rotary" : "Joystick");
  ESP_LOGCONFIG(TAG, "Active: %s", YESNO(this->active_));
  ESP_LOGCONFIG(TAG, "Menu items:");
  for (size_t i = 0; i < this->displayed_item_->items_size(); i++) {
    auto *item = this->displayed_item_->get_item(i);
    ESP_LOGCONFIG(TAG, "  %i: %s (Type: %s, Immediate Edit: %s)", i, item->get_text().c_str(),
                  LOG_STR_ARG(display_menu_base::menu_item_type_to_string(item->get_type())),
                  YESNO(item->get_immediate_edit()));
  }
}

void GraphicalDisplayMenu::set_display(display::Display *display) { this->display_ = display; }

void GraphicalDisplayMenu::set_font(display::BaseFont *font) { this->font_ = font; }

void GraphicalDisplayMenu::set_foreground_color(Color foreground_color) { this->foreground_color_ = foreground_color; }
void GraphicalDisplayMenu::set_background_color(Color background_color) { this->background_color_ = background_color; }

void GraphicalDisplayMenu::on_before_show() {
  if (this->display_ != nullptr) {
    this->previous_display_page_ = this->display_->get_active_page();
    this->display_->show_page(this->display_page_.get());
    this->display_->clear();
  } else {
    this->update();
  }
}

void GraphicalDisplayMenu::on_before_hide() {
  if (this->previous_display_page_ != nullptr) {
    this->display_->show_page((display::DisplayPage *) this->previous_display_page_);
    this->display_->clear();
    this->update();
    this->previous_display_page_ = nullptr;
  } else {
    this->update();
  }
}

void GraphicalDisplayMenu::draw_and_update() {
  this->update();

  // If we're in advanced drawing mode we won't have a display and will instead require the update callback to do
  // our drawing
  if (this->display_ != nullptr) {
    draw_menu();
  }
}

void GraphicalDisplayMenu::draw_menu() {
  if (this->display_ == nullptr) {
    ESP_LOGE(TAG, "draw_menu() called without a display_. This is only available when using the menu in pop up mode");
    return;
  }
  display::Rect bounds(0, 0, this->display_->get_width(), this->display_->get_height());
  this->draw_menu_internal_(this->display_, &bounds);
}

void GraphicalDisplayMenu::draw(display::Display *display, const display::Rect *bounds) {
  this->draw_menu_internal_(display, bounds);
}

void GraphicalDisplayMenu::draw_menu_internal_(display::Display *display, const display::Rect *bounds) {
  int total_height = 0;
  int y_padding = 2;
  bool scroll_menu_items = false;
  std::vector<display::Rect> menu_dimensions;
  int number_items_fit_to_screen = 0;
  const int max_item_index = this->displayed_item_->items_size() - 1;

  for (size_t i = 0; i <= max_item_index; i++) {
    const auto *item = this->displayed_item_->get_item(i);
    const bool selected = i == this->cursor_index_;
    const display::Rect item_dimensions = this->measure_item(display, item, bounds, selected);

    menu_dimensions.push_back(item_dimensions);
    total_height += item_dimensions.h + (i == 0 ? 0 : y_padding);

    if (total_height <= bounds->h) {
      number_items_fit_to_screen++;
    } else {
      // Scroll the display if the selected item or the item immediately after it overflows
      if ((selected) || (i == this->cursor_index_ + 1)) {
        scroll_menu_items = true;
      }
    }
  }

  // Determine what items to draw
  int first_item_index = 0;
  int last_item_index = max_item_index;

  if (number_items_fit_to_screen <= 1) {
    // If only one item can fit to the bounds draw the current cursor item
    last_item_index = std::min(last_item_index, this->cursor_index_ + 1);
    first_item_index = this->cursor_index_;
  } else {
    if (scroll_menu_items) {
      // Attempt to draw the item after the current item (+1 for equality check in the draw loop)
      last_item_index = std::min(last_item_index, this->cursor_index_ + 1);

      // Go back through the measurements to determine how many prior items we can fit
      int height_left_to_use = bounds->h;
      for (int i = last_item_index; i >= 0; i--) {
        const display::Rect item_dimensions = menu_dimensions[i];
        height_left_to_use -= (item_dimensions.h + y_padding);

        if (height_left_to_use <= 0) {
          // Ran out of space -  this is our first item to draw
          first_item_index = i;
          break;
        }
      }
      const int items_to_draw = last_item_index - first_item_index;
      // Dont't draw last item partially if it is the selected item
      if ((this->cursor_index_ == last_item_index) && (number_items_fit_to_screen <= items_to_draw) &&
          (first_item_index < max_item_index)) {
        first_item_index++;
      }
    }
  }

  // Render the items into the view port
  display->start_clipping(*bounds);

  int y_offset = bounds->y;
  for (size_t i = first_item_index; i <= last_item_index; i++) {
    const auto *item = this->displayed_item_->get_item(i);
    const bool selected = i == this->cursor_index_;
    display::Rect dimensions = menu_dimensions[i];

    dimensions.y = y_offset;
    dimensions.x = bounds->x;
    this->draw_item(display, item, &dimensions, selected);

    y_offset = dimensions.y + dimensions.h + y_padding;
  }

  display->end_clipping();
}

display::Rect GraphicalDisplayMenu::measure_item(display::Display *display, const display_menu_base::MenuItem *item,
                                                 const display::Rect *bounds, const bool selected) {
  display::Rect dimensions(0, 0, 0, 0);

  if (selected) {
    // TODO: Support selection glyph
    dimensions.w += 0;
    dimensions.h += 0;
  }

  std::string label = item->get_text();
  if (item->has_value()) {
    // Append to label
    MenuItemValueArguments args(item, selected, this->editing_);
    label.append(this->menu_item_value_.value(&args));
  }

  int x1;
  int y1;
  int width;
  int height;
  display->get_text_bounds(0, 0, label.c_str(), this->font_, display::TextAlign::TOP_LEFT, &x1, &y1, &width, &height);

  dimensions.w = std::min((int16_t) width, bounds->w);
  dimensions.h = std::min((int16_t) height, bounds->h);

  return dimensions;
}

inline void GraphicalDisplayMenu::draw_item(display::Display *display, const display_menu_base::MenuItem *item,
                                            const display::Rect *bounds, const bool selected) {
  const auto background_color = selected ? this->foreground_color_ : this->background_color_;
  const auto foreground_color = selected ? this->background_color_ : this->foreground_color_;

  // int background_width = std::max(bounds->width, available_width);
  int background_width = bounds->w;

  if (selected) {
    display->filled_rectangle(bounds->x, bounds->y, background_width, bounds->h, background_color);
  }

  std::string label = item->get_text();
  if (item->has_value()) {
    MenuItemValueArguments args(item, selected, this->editing_);
    label.append(this->menu_item_value_.value(&args));
  }

  display->print(bounds->x, bounds->y, this->font_, foreground_color, display::TextAlign::TOP_LEFT, label.c_str(),
                 ~foreground_color);
}

void GraphicalDisplayMenu::draw_item(const display_menu_base::MenuItem *item, const uint8_t row, const bool selected) {
  ESP_LOGE(TAG, "draw_item(MenuItem *item, uint8_t row, bool selected) called. The graphical_display_menu specific "
                "draw_item should be called.");
}

void GraphicalDisplayMenu::update() { this->on_redraw_callbacks_.call(); }

}  // namespace graphical_display_menu
}  // namespace esphome
