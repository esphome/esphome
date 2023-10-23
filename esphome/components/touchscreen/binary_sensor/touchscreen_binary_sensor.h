#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/display/display_buffer.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace touchscreen {

class TouchscreenBinarySensor : public binary_sensor::BinarySensor,
                                public Component,
                                public TouchListener,
                                public Parented<Touchscreen> {
 public:
  void setup() override;

  /// Set the touch screen area where the button will detect the touch.
  void set_area(int16_t x_min, int16_t x_max, int16_t y_min, int16_t y_max) {
    this->x_min_ = x_min;
    this->x_max_ = x_max;
    this->y_min_ = y_min;
    this->y_max_ = y_max;
  }
  int16_t get_x_min() { return this->x_min_; }
  int16_t get_x_max() { return this->x_max_; }
  int16_t get_y_min() { return this->y_min_; }
  int16_t get_y_max() { return this->y_max_; }
  int16_t get_width() { return this->x_max_ - this->x_min_; }
  int16_t get_height() { return this->y_max_ - this->y_min_; }

  void set_page(display::DisplayPage *page) { this->page_ = page; }

  void touch(TouchPoint tp) override;
  void release() override;

 protected:
  int16_t x_min_, x_max_, y_min_, y_max_;
  display::DisplayPage *page_{nullptr};
};

}  // namespace touchscreen
}  // namespace esphome
