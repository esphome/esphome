#include "pipsolar_switch.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome::pipsolar {

ESPHOME_LOG_TAG(TAG, "pipsolar.switch");

void PipsolarSwitch::dump_config() { LOG_SWITCH("", "Pipsolar Switch", this); }
void PipsolarSwitch::write_state(bool state) {
  const char *command = state ? this->on_command_ : this->off_command_;
  if (command != nullptr) {
    this->parent_->queue_command(command);
  }
}

}  // namespace esphome::pipsolar
