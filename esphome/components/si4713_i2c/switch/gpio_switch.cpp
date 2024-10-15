#include "gpio_switch.h"

namespace esphome {
namespace si4713 {

void GPIOSwitch::write_state(bool value) {
  this->publish_state(value);
  this->parent_->set_gpio(this->pin_, value);
}

}  // namespace si4713
}  // namespace esphome
