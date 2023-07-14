#include "micronova_button.h"

namespace esphome {
namespace micronova {

void MicroNovaButton::press_action() {
  uint8_t new_temp = 20;

  switch (this->get_function()) {
    case MicroNovaFunctions::STOVE_FUNCTION_TEMP_UP:
    case MicroNovaFunctions::STOVE_FUNCTION_TEMP_DOWN:
      new_temp = this->micronova_->get_thermostat_temperature() +
                 (MicroNovaFunctions::STOVE_FUNCTION_TEMP_UP == this->get_function() ? 1 : -1);
      this->micronova_->write_address(this->memory_location_, this->memory_address_, new_temp);
      // force update the stove sensors to reflect the new thermostat value
      this->micronova_->update();
      break;

    default:
      break;
  }
}

}  // namespace micronova
}  // namespace esphome
