#pragma once

#include "esphome/core/defines.h"
#ifdef USE_DISPLAY
#include "esphome/components/display/display_buffer.h"
#endif

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
#ifdef USE_DISPLAY
  void set_display(display::Display *display);
  display::Display *get_display() const { return this->display_; }
#else
  void set_display_dimension(uint16_t width, u_int16_t height) {
    this->display_width_ = width;
    this->display_height_ = height;
  }
  void set_rotation(TouchRotation rotation) { this->rotation_ = rotation; }
#endif
  Trigger<TouchPoint> *get_touch_trigger() { return &this->touch_trigger_; }

  void register_listener(TouchListener *listener) { this->touch_listeners_.push_back(listener); }

 protected:
  /// Call this function to send touch points to the `on_touch` listener and the binary_sensors.
  void send_touch_(TouchPoint tp);

  uint16_t get_width_() {
#ifdef USE_DISPLAY
    if (this->display_ != nullptr) {
      return this->display_->get_width_internal();
    } else {
      return 100;
    }
#else
    return display_width_;
#endif
  }
  uint16_t get_height_() {
#ifdef USE_DISPLAY
    if (this->display_ != nullptr) {
      return display_->get_height_internal();
    } else {
      return 100;
    }
#else
    return display_height_;
#endif
  }

  TouchRotation get_rotation_() {
#ifdef USE_DISPLAY
    if (this->display_ != nullptr) {
      return static_cast<TouchRotation>(this->display_->get_rotation());
    } else {
      return ROTATE_0_DEGREES;
    }
#else
    return this->rotation_;
#endif
  }

#ifdef USE_DISPLAY
  display::Display *display_{nullptr};
#else
  uint16_t display_width_{240};
  uint16_t display_height_{320};

  TouchRotation rotation_{ROTATE_0_DEGREES};
#endif

  Trigger<TouchPoint> touch_trigger_;
  std::vector<TouchListener *> touch_listeners_;
};

}  // namespace touchscreen
}  // namespace esphome
