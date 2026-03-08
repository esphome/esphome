#include "micronova_button.h"

namespace esphome::micronova {

void MicroNovaButton::press_action() {
  this->micronova_->write_address(this->memory_location_, this->memory_address_, this->memory_data_);
  this->micronova_->request_update_listeners();
}

}  // namespace esphome::micronova
