#include "micronova_button.h"

namespace esphome::micronova {

void MicroNovaButton::press_action() {
  switch (this->get_function()) {
    case MicroNovaFunctions::STOVE_FUNCTION_CUSTOM:
      this->micronova_->queue_write_command(this->memory_location_, this->memory_address_, this->memory_data_);
      break;
    default:
      break;
  }
}

}  // namespace esphome::micronova
