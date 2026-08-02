#include "micronova_button.h"

namespace esphome::micronova {

static const char *const TAG = "micronova.button";

void MicroNovaButton::dump_config() {
  LOG_BUTTON("", "Micronova button", this);
  this->dump_base_config();
}

void MicroNovaButton::press_action() {
  this->micronova_->queue_write_command(this->memory_location_, this->memory_address_, this->memory_data_);
}

}  // namespace esphome::micronova
