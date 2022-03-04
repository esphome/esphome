#include "widgets.h"

#include <utility>
#include "esphome/core/application.h"
#include "esphome/core/color.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace display {

  int Widget::get_width() {
    return 0;
  }
  int Widget::get_height() {
    return 0;
  }

  void Widget::draw(DisplayBuffer* it) {
    return draw(it, 0, 0, it.get_width(), it.get_height());
  }

  void WidgetContainer::draw(DisplayBuffer* it, int x1, int y1, int width, int height) {
    for (auto *widget : children_) {
      widget->draw(it, x1, y1, width, height);
    }
  }

  int Horizontal::get_width() {
    int ret = 0;
    for (auto *widget : children_) {
      ret += widget->get_width();
    }
    return ret;
  }

  void Horizontal::draw(DisplayBuffer* it, int x1, int y1, int width, int height) {
    for (auto *widget : children_) {
      width1 = widget->get_width();
      widget->draw(it, x1, y1, width1, height);
      x1 += width1;
    }
  }

  int Vertical::get_height() {
    int ret = 0;
    for (auto *widget : children_) {
      ret += widget->get_height();
    }
    return ret;
  }

  void Vertical::draw(DisplayBuffer* it, int x1, int y1, int width, int height) {
    for (auto *widget : children_) {
      height1 = widget->get_height();
      widget->draw(it, x1, y1, width, height1);
      y1 += height1;
    }
  }

}  // namespace display
}  // namespace esphome
