#pragma once

#include "esphome/components/switch/switch.h"
#include "../si4713.h"

namespace esphome {
namespace si4713 {

class GPIOSwitch : public switch_::Switch, public Parented<Si4713Component> {
 public:
  GPIOSwitch() = default;

  void set_pin(uint8_t pin) { this->pin_ = pin - 1; }

 protected:
  void write_state(bool value) override;

  uint8_t pin_{0};
};

}  // namespace si4713
}  // namespace esphome
