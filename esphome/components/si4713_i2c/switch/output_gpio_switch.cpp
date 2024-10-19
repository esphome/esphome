#include "output_gpio_switch.h"

namespace esphome {
namespace si4713 {

void OutputGpioSwitch::write_state(bool value) {
  this->publish_state(value);
  this->parent_->set_output_gpio(this->pin_, value);
}

}  // namespace si4713
}  // namespace esphome
