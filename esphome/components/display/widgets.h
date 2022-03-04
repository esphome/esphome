#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"

#include "esphome/components/display/display_buffer.h"

namespace esphome {
namespace display {

class Widget {
 public:
  virtual int get_width();
  virtual int get_height();

  virtual void draw(DisplayBuffer* it, int x1, int y1, int width, int height);

  void draw(DisplayBuffer* it);
};

class WidgetContainer : public Widget {
 public:
  virtual void draw(DisplayBuffer* it, int x1, int y1, int width, int height);

  void set_children(std::vector<Widget *> children) {
    children_ = children;
  };
 protected:
  std::vector<Widget *> children;
};

class Horizontal : public WidgetContainer {
  virtual int get_width();

  virtual void draw(DisplayBuffer* it, int x1, int y1, int width, int height);
};

 class Text : public Widget {
   

}  // namespace display
}  // namespace esphome
