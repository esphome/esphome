#include "micronova_button.h"

namespace esphome::micronova {

void MicroNovaButton::press_action() {
  this->micronova_->queue_write_command(this->memory_location_, this->memory_address_, this->memory_data_);
}

}  // namespace esphome::micronova
