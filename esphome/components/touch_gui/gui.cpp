#include "gui.h"
#include "esphome/core/log.h"
#include <algorithm>

namespace esphome {
namespace touch_gui {

static const char *TAG = "touch_gui";

void TouchGUIComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Configured");
  ESP_LOGCONFIG(TAG, "  Objects: %zu", drawables_.size());
  ESP_LOGCONFIG(TAG, "  Radio groups: %zu", groups_.size());
}

void TouchGUIComponent::add_on_update_callback(std::function<void()> &&callback) {
  this->update_callback_.add(std::move(callback));
}

void TouchGUIComponent::setup() {
  // Ensure exactly once activated radio group button in every group
  RadioGroupMap activated;
  static const ButtonPtrSet EMPTY_SET;

  // Build a map of groups
  for (auto *d : this->drawables_) {
    if (d->is_button()) {
      TouchGUIButton *b = static_cast<TouchGUIButton *>(d);
      if (b->get_type() == TOUCH_GUI_BUTTON_TYPE_RADIO) {
        auto it = this->groups_.insert(std::pair<int, ButtonPtrSet>(b->get_radio_group(), EMPTY_SET)).first;
        it->second.insert(b);
        it = activated.insert(std::pair<int, ButtonPtrSet>(b->get_radio_group(), EMPTY_SET)).first;
        if (b->get_initial())
          it->second.insert(b);
      }
    }
  }

  for (const auto &a : activated) {
    // If none is active, activate the first one
    if (a.second.empty()) {
      auto &g = this->groups_[a.first];
      if (!g.empty()) {
        (*g.begin())->set_initial(true);
      }
      // If more than one is active, leave the first one
    } else if (a.second.size() > 1) {
      auto it = a.second.begin();
      ++it;
      for (; it != a.second.end(); ++it)
        (*it)->set_initial(false);
    }
  }
}

void TouchGUIComponent::update() {
  bool upd = false;

  this->check_page_changed_();

  for (auto *d : this->drawables_) {
    d->update_drawable();
    upd |= d->should_update();
  }

  if (upd)
    this->update_callback_.call();
}

void TouchGUIComponent::draw() {
  this->check_page_changed_();

  for (auto *d : this->drawables_)
    if (d->is_visible())
      d->draw();
}

void TouchGUIComponent::touch(int x, int y, bool touched) {
  if (touched) {
    // Create a list of interacted buttons first and activate in the second step.
    // Otherwise a touch changing a page could get immediately registered in the
    // second page as well and an unintended button gets activated.
    std::vector<TouchGUIButton *> interacted;

    for (auto *d : this->drawables_) {
      if (d->is_button()) {
        TouchGUIButton *b = static_cast<TouchGUIButton *>(d);
        if (b->is_interacted(x, y))
          interacted.push_back(b);
      }
    }

    for (auto *b : interacted) {
      activate(b);
      b->set_touch_state(true);
    }
  } else
    release_();
}

void TouchGUIComponent::activate(TouchGUIButton *button) {
  switch (button->get_type()) {
    case TOUCH_GUI_BUTTON_TYPE_TOGGLE:
      button->publish_state(!button->state);
      break;
    case TOUCH_GUI_BUTTON_TYPE_RADIO:
      if (!button->state) {
        // Find the radio group, deactivate the previous one, activate the new one
        auto it = this->groups_.find(button->get_radio_group());
        if (it != this->groups_.end()) {
          for (auto *b : it->second) {
            if (b->state) {
              b->publish_state(false);
            }
          }

          button->publish_state(true);
        }
      }
      break;
    case TOUCH_GUI_BUTTON_TYPE_MOMENTARY:
    case TOUCH_GUI_BUTTON_TYPE_AREA:
      button->publish_state(true);
      break;
    default:
      break;
  }
}

void TouchGUIComponent::release_() {
  for (auto *d : this->drawables_) {
    if (d->is_button()) {
      TouchGUIButton *b = static_cast<TouchGUIButton *>(d);

      switch (b->get_type()) {
        case TOUCH_GUI_BUTTON_TYPE_MOMENTARY:
        case TOUCH_GUI_BUTTON_TYPE_AREA:
          // A filter might have set our state to false but still needs to be informed
          // when the touch is released
          if (b->state || b->is_touched())
            b->publish_state(false);
          break;
        default:
          break;
      }

      b->set_touch_state(false);
    }
  }
}

void TouchGUIComponent::check_page_changed_() {
  if (this->display_->get_active_page() != this->last_page_displayed_) {
    // All activated momentary and area buttons are released
    release_();
    this->last_page_displayed_ = this->display_->get_active_page();
  }
}

bool TouchGUIDrawable::is_visible() const {
  // The button is visible if either the list of the pages is empty (meaning all)
  // or the displayed page matches
  return (pages_.empty() ||
          std::find(pages_.begin(), pages_.end(), parent_->get_display()->get_active_page()) != pages_.end());
}

void TouchGUIButton::setup() {
  publish_initial_state(this->initial_);

  // If momentary we need to know of the activation to schedule the release
  switch (this->type_) {
    case TOUCH_GUI_BUTTON_TYPE_MOMENTARY:
    case TOUCH_GUI_BUTTON_TYPE_AREA:
      add_on_state_callback([this](bool x) {
        if (x)
          this->activation_time_ = millis();
      });
      break;
    default:
      break;
  }

  // Mark the changed so the display can be asked to update
  add_on_state_callback([this](bool x) { this->update_requested_ = true; });
}

void TouchGUIButton::update_drawable() {
  // If this is a momentary button, return it after its touch time expires
  switch (this->type_) {
    case TOUCH_GUI_BUTTON_TYPE_MOMENTARY:
    case TOUCH_GUI_BUTTON_TYPE_AREA:
      if (this->state && millis() - this->activation_time_ >= this->touch_time_) {
        publish_state(false);
        this->update_requested_ = true;
      }
      break;
    default:
      break;
  }
}

void TouchGUIButton::draw() {
  if (this->writer_.has_value())
    (this->writer_.value())(*this);
  else if (this->parent_->get_writer().has_value()) {
    (this->parent_->get_writer().value())(*this);
  } else {
    if (this->type_ != TOUCH_GUI_BUTTON_TYPE_AREA) {
      get_display()->rectangle(this->x_min_, this->y_min_, this->x_max_ - this->x_min_ + 1,
                               this->y_max_ - this->y_min_ + 1, get_border_color());
      get_display()->filled_rectangle(this->x_min_ + 1, this->y_min_ + 1, this->x_max_ - this->x_min_ - 1,
                                      this->y_max_ - this->y_min_ - 1, get_background_color_to_use());
      if (get_font() != nullptr && !this->text_.value().empty())
        get_display()->print(get_x_center(), get_y_center(), get_font(), get_foreground_color_to_use(),
                             display::TextAlign::CENTER, this->text_.value().c_str());
    }
  }
}

}  // namespace touch_gui
}  // namespace esphome
