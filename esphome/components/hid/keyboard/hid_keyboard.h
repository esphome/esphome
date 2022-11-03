#pragma once

#include <vector>
#include <cstdint>
#include <functional>
#include "esphome/components/keyboard/keyboard.h"

namespace esphome {
namespace hid {

class Keyboard : public keyboard::KeyboardControl {
 public:
  void control(std::vector<uint16_t> &&keys) override;

  void set_parrent(keyboard::LEDControl *led_control) { led_control_ = led_control; }

  static void set_parrent(Keyboard *thiz, keyboard::LEDControl *led_control);

 protected:
  virtual void report() = 0;

  static constexpr uint8_t ROLLOVER = 6;
  uint8_t modifier_{0};
  uint8_t hidcode_[ROLLOVER]{
      0,
  };
  keyboard::LEDControl *led_control_{nullptr};
};

}  // namespace hid
}  // namespace esphome
