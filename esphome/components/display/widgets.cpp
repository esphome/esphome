#include "widgets.h"

#include <utility>
#include "esphome/core/application.h"
#include "esphome/core/color.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

static const char* TAG = "display.widgets";

namespace esphome {
namespace display {

  /*
    All widgets need to declare their preferred size by implementing get_size.
    Container widgets need to calculate their preferred size from their child widgets' sizes.
  */

  void Widget::get_size(int *width, int *height) {
    *width = *height = 0;
  }

  void Widget::draw_fullscreen(DisplayBuffer& it) {
    int width, height;
    // Trigger sizing
    get_size(&width, &height);
    ESP_LOGV(TAG, "Minimum size is (%d, %d)", width, height);

    it.fill(COLOR_ON);
    return draw(&it, 0, 0, it.get_width(), it.get_height());
  }

  void WidgetContainer::get_size(int *width, int *height) {
    *width = 0, *height = 0;
    for (auto& child : children_) {
        child.widget_->get_size(&child.preferred_width, &child.preferred_height);
        *width = std::max(*width, child.preferred_width);
        *height = std::max(*height, child.preferred_height);
    }
  }

  void WidgetContainer::draw(DisplayBuffer* it, int x1, int y1, int width, int height) {
    for (auto child : children_) {
      child.widget_->draw(it, x1, y1, width, height);
    }
  }

  void Horizontal::get_size(int *width, int *height) {
    WidgetContainer::get_size(width, height);
    *width = 0;
    for (auto child : children_) {
      *width += child.preferred_width;
    }
  }

  void Horizontal::draw(DisplayBuffer* it, int x1, int y1, int width, int height) {
    ESP_LOGV(TAG, "Horizontal::draw at (%d, %d, %d, %d)", x1, y1, width, height);
    for (auto child : children_) {
      ESP_LOGV(TAG, "Drawing horizontal child at (%d, %d, %d, %d)", x1, y1, child.preferred_width, height);
      child.widget_->draw(it, x1, y1, child.preferred_width, height);
      x1 += child.preferred_width;
    }
  }

  void Vertical::get_size(int *width, int *height) {
    WidgetContainer::get_size(width, height);
    *height = 0;
    for (auto child : children_) {
      *height += child.preferred_height;
    }
  }

  void Vertical::draw(DisplayBuffer* it, int x1, int y1, int width, int height) {
    ESP_LOGV(TAG, "Vertical::draw at (%d, %d, %d, %d)", x1, y1, width, height);
    for (auto child : children_) {
      ESP_LOGV(TAG, "Drawing vertical child at (%d, %d, %d, %d)", x1, y1, width, child.preferred_height);
      child.widget_->draw(it, x1, y1, width, child.preferred_height);
      y1 += child.preferred_height;
    }
  }

  template<> void Text<>::calculate_text_() {
    std::string text = text_.value();
    if (source_ != NULL && source_->has_state()) {
      cached_text_.resize(256);
      snprintf(&cached_text_[0], cached_text_.size(), text.c_str(), source_->state);
    } else if (source_text_ != NULL && source_text_->has_state()) {
      cached_text_.resize(256);
      snprintf(&cached_text_[0], cached_text_.size(), text.c_str(), source_text_->state.c_str());
    } else {
      cached_text_ = text;
    }
  }
  template<> void Text<>::get_size(int *width, int *height) {
    calculate_text_();
    int unused_x_offset, unused_baseline;
    font_->measure(cached_text_.c_str(), width, &unused_x_offset, &unused_baseline, height);
  }

  template<> void Text<>::draw(DisplayBuffer* it, int x1, int y1, int width, int height) {
    it->print(x1, y1, font_, COLOR_OFF, cached_text_.c_str());
  }

}  // namespace display
}  // namespace esphome
