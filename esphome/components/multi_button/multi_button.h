#pragma once

/** This is a component that allows for receiving simple events from a button connected to a GPIO
 * It supports multiple button presses and/or press&hold and signals them using a numerical code
 * Credits go to @mathertel because his OneButton heavily inspired this work
 */

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace multi_button {

class MultiButton : public sensor::Sensor, public Component {
 public:
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void setup() override;
  void dump_config() override;
  void loop() override;

  void set_pin(GPIOPin *pin) { this->pin_ = pin; }
  void set_debounce(const int millis) { this->debounce_millis_ = millis; }
  void set_timeout(const int millis) { this->timeout_millis_ = millis; }
  void set_press_hold_threshold(const int millis) { this->press_hold_threshold_millis_ = millis; }
  void set_press_hold_update_rate(const float frequency) {
    this->press_hold_update_interval_millis_ = (int) (1000.0f / frequency);
  }

 protected:
  enum class MultiButtonState {
    RELEASED = 0,
    MAYBE_PRESSED = 1,
    PRESSED = 2,
    MAYBE_RELEASED = 3,
    PRESS_HOLD = 4,
    MAYBE_NOT_PRESS_HOLD = 5
  };

  GPIOPin *pin_;
  // The time in [ms] a button state needs to be stable to be recognized and not ignored as bouncing
  int debounce_millis_;
  // The timeout in [ms] before an event is recognized and published. A double press must have a smaller pause than this
  // between two presses
  int timeout_millis_;
  // The threshold in [ms] where a normal press becomes a press&hold
  int press_hold_threshold_millis_;
  // The interval in [ms] at which updates are being published during a press&hold
  int press_hold_update_interval_millis_;

  MultiButtonState state_;
  unsigned long start_time_;
  bool level_;
  int press_count_;
  int hold_count_;
  bool reset_;

  void tick_(unsigned long time, bool level);
};

}  // namespace multi_button
}  // namespace esphome
