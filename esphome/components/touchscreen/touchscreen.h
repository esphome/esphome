#pragma once

#include "esphome/core/defines.h"
#include "esphome/components/display/display_buffer.h"

#include "esphome/core/automation.h"
#include "esphome/core/hal.h"

#include <vector>
#include <map>

namespace esphome {
namespace touchscreen {

struct TouchPoint {
  uint8_t id;
  int16_t x_raw{0}, y_raw{0}, z_raw{0};
  uint16_t x_prev{0}, y_prev{0};
  uint16_t x_org{0}, y_org{0};
  uint16_t x{0}, y{0};
  uint8_t state{0};
};

using TouchPoints_t = std::vector<TouchPoint>;

struct TouchscreenInterupt {
  volatile bool touched{true};
  bool init{false};
  static void gpio_intr(TouchscreenInterupt *store);
};

class TouchListener {
 public:
  virtual void touch(TouchPoint tp) {}
  virtual void touch(const TouchPoints_t &tpoints) {}
  virtual void update(const TouchPoints_t &tpoints) {}
  virtual void release() {}
};

enum TouchRotation {
  ROTATE_0_DEGREES = 0,
  ROTATE_90_DEGREES = 90,
  ROTATE_180_DEGREES = 180,
  ROTATE_270_DEGREES = 270,
};

class Touchscreen : public PollingComponent {
 public:
  void set_display(display::Display *display) { this->display_ = display; }
  display::Display *get_display() const { return this->display_; }

  void set_swap_x_y(bool swap) { this->swap_x_y_ = swap; }
  void set_calibration(int16_t x_min, int16_t x_max, int16_t y_min, int16_t y_max) {
    this->x_raw_min_ = std::min(x_min, x_max);
    this->x_raw_max_ = std::max(x_min, x_max);
    this->y_raw_min_ = std::min(y_min, y_max);
    this->y_raw_max_ = std::max(y_min, y_max);
    this->invert_x_ = (x_min > x_max);
    this->invert_y_ = (y_min > y_max);
  }

  Trigger<TouchPoint, const TouchPoints_t &> *get_touch_trigger() { return &this->touch_trigger_; }
  Trigger<const TouchPoints_t &> *get_update_trigger() { return &this->update_trigger_; }
  Trigger<> *get_release_trigger() { return &this->release_trigger_; }

  void register_listener(TouchListener *listener) { this->touch_listeners_.push_back(listener); }

  virtual void update_touches() = 0;

  void update() override;
  void loop() override;

 protected:
  /// Call this function to send touch points to the `on_touch` listener and the binary_sensors.

  void attach_interrupt_(InternalGPIOPin *irq_pin, esphome::gpio::InterruptType type);

  void set_raw_touch_posistion_(uint8_t id, int16_t x_raw, int16_t y_raw, int16_t z_raw = 0);

  void send_touches_();

  static int16_t normalize(int16_t val, int16_t min_val, int16_t max_val, bool inverted = false);

  uint16_t get_width_() { return this->display_->get_width(); }

  uint16_t get_height_() { return this->display_->get_height(); }

  TouchRotation get_rotation_() { return static_cast<TouchRotation>(this->display_->get_rotation()); }
  display::Display *display_{nullptr};

  int16_t x_raw_min_{0}, x_raw_max_{0}, y_raw_min_{0}, y_raw_max_{0};
  bool invert_x_{false}, invert_y_{false}, swap_x_y_{false};

  Trigger<TouchPoint, const TouchPoints_t &> touch_trigger_;
  Trigger<const TouchPoints_t &> update_trigger_;
  Trigger<> release_trigger_;
  std::vector<TouchListener *> touch_listeners_;

  std::map<uint8_t, TouchPoint> touches_;
  TouchscreenInterupt store_;

  bool first_touch_{true};
  bool need_update_{false};
  bool is_touched_{false};
};

}  // namespace touchscreen
}  // namespace esphome
