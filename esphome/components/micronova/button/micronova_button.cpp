#include "micronova_button.h"

namespace esphome::micronova {

void MicroNovaButton::press_action() {
  switch (this->get_function()) {
    case MicroNovaFunctions::STOVE_FUNCTION_CUSTOM:
      this->micronova_->write_address(this->memory_location_, this->memory_address_, this->memory_data_);
      break;
    default:
      break;
  }
  this->micronova_->update();
}

}  // namespace esphome::micronova
