#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"

#include "esphome/components/display/display_buffer.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome {
namespace display {

class Widget {
 public:
  virtual void get_size(int *width, int *height);

  virtual void draw(DisplayBuffer* it, int x1, int y1, int width, int height);

  void draw_fullscreen(DisplayBuffer& it);
};

class WidgetContainer : public Widget {
 public:
  virtual void get_size(int *width, int *height);
  virtual void draw(DisplayBuffer* it, int x1, int y1, int width, int height);

  void set_children(std::vector<Widget *> children) {
    children_.clear();
    std::for_each(begin(children), end(children), [this](Widget* widget) {
      children_.emplace_back(Child(widget));
    });
  };

 protected:
  class Child {
  public:
    Child(Widget* widget) : widget_(widget) {}
    Widget *widget_;
    int x, y, width, height;
    int preferred_width, preferred_height;
  };
  std::vector<Child> children_;
};

class Horizontal : public WidgetContainer {
public:
  virtual void get_size(int *width, int *height);

  virtual void draw(DisplayBuffer* it, int x1, int y1, int width, int height);
};

class Vertical : public WidgetContainer {
public:
  virtual void get_size(int *width, int *height);

   virtual void draw(DisplayBuffer* it, int x1, int y1, int width, int height);
 };

template<typename... Ts>  class Text : public Widget {
 public:
   TEMPLATABLE_VALUE(std::string, text);
  void set_font(Font* font) { font_ = font; }

  virtual void get_size(int *width, int *height);

  virtual void draw(DisplayBuffer* it, int x1, int y1, int width, int height);

  void set_sensor(sensor::Sensor *sensor) { source_ = sensor; source_text_ = NULL; }
  void set_sensor(text_sensor::TextSensor *sensor) { source_text_ = sensor; source_ = NULL; }

 protected:
  Font* font_;
  void calculate_text_();
  std::string cached_text_;
  sensor::Sensor *source_;
  text_sensor::TextSensor *source_text_;
};

}  // namespace display
}  // namespace esphome
