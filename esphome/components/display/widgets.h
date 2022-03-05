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
  std::vector<Widget *> children_;
};

class Horizontal : public WidgetContainer {
 public:
  virtual int get_width();

  virtual void draw(DisplayBuffer* it, int x1, int y1, int width, int height);
};

class Vertical : public WidgetContainer {
 public:
   virtual int get_height();

   virtual void draw(DisplayBuffer* it, int x1, int y1, int width, int height);
 };

 class Text : public Widget {
 public:
   TEMPLATABLE_VALUE(const char*, text);
   void set_font(Font* font);

   virtual int get_width();
   virtual int get_height();

   virtual void draw(DisplayBuffer* it, int x1, int y1, int width, int height);
 protected:
   const char* text_;
   Font* font_;
 }

}  // namespace display
}  // namespace esphome
