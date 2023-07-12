#pragma once

#include "esphome/components/display/display_buffer.h"
#include "esphome/core/automation.h"
#include "esphome/core/hal.h"

#include <vector>

namespace esphome {
namespace touchscreen {

struct TouchPoint {
  uint16_t x;
  uint16_t y;
  uint8_t id;
  uint8_t state;
};

class TouchListener {
 public:
  virtual void touch(TouchPoint tp) = 0;
  virtual void release() {}
};

enum TouchRotation {
  ROTATE_0_DEGREES = 0,
  ROTATE_90_DEGREES = 90,
  ROTATE_180_DEGREES = 180,
  ROTATE_270_DEGREES = 270,
};

class Touchscreen {
 public:
  void set_display(display::Display *display);
  display::Display *get_display() const { return this->display_; }

  Trigger<TouchPoint> *get_touch_trigger() { return &this->touch_trigger_; }

  void register_listener(TouchListener *listener) { this->touch_listeners_.push_back(listener); }

 protected:
  /// Call this function to send touch points to the `on_touch` listener and the binary_sensors.
  void send_touch_(TouchPoint tp);

  uint16_t display_width_;
  uint16_t display_height_;
  display::Display *display_;
  TouchRotation rotation_;
  Trigger<TouchPoint> touch_trigger_;
  std::vector<TouchListener *> touch_listeners_;
};

}  // namespace touchscreen
}  // namespace esphome
