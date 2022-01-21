#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace ektf2232 {

struct EKTF2232TouchscreenStore {
  volatile bool touch;
  ISRInternalGPIOPin pin;

  static void gpio_intr(EKTF2232TouchscreenStore *store);
};

struct TouchPoint {
  uint16_t x;
  uint16_t y;
};

class TouchListener {
 public:
  virtual void touch(TouchPoint tp) = 0;
  virtual void release();
};

enum EKTF2232Rotation : uint8_t {
  ROTATE_0_DEGREES = 0,
  ROTATE_90_DEGREES,
  ROTATE_180_DEGREES,
  ROTATE_270_DEGREES,
};

class EKTF2232Touchscreen : public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_rts_pin(GPIOPin *pin) { this->rts_pin_ = pin; }

  void set_display_details(uint16_t width, uint16_t height, EKTF2232Rotation rotation) {
    this->display_width_ = width;
    this->display_height_ = height;
    this->rotation_ = rotation;
  }

  void set_power_state(bool enable);
  bool get_power_state();

  Trigger<TouchPoint> *get_touch_trigger() const { return this->touch_trigger_; }

  void register_listener(TouchListener *listener) { this->touch_listeners_.push_back(listener); }

 protected:
  void hard_reset_();
  bool soft_reset_();

  InternalGPIOPin *interrupt_pin_;
  GPIOPin *rts_pin_;
  EKTF2232TouchscreenStore store_;
  uint16_t x_resolution_;
  uint16_t y_resolution_;

  uint16_t display_width_;
  uint16_t display_height_;
  EKTF2232Rotation rotation_;
  Trigger<TouchPoint> *touch_trigger_ = new Trigger<TouchPoint>();
  std::vector<TouchListener *> touch_listeners_;
};

}  // namespace ektf2232
}  // namespace esphome
