#include "gree_switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace gree {

static const char *const TAG = "gree.switch";

void GreeModeBitSwitch::setup() {
  auto initial = this->get_initial_state_with_restore_mode();
  if (initial.has_value()) {
    this->write_state(*initial);
  }
}

void GreeModeBitSwitch::dump_config() { log_switch(TAG, "  ", this->name_, this); }

void GreeModeBitSwitch::write_state(bool state) {
  this->parent_->set_mode_bit(this->bit_mask_, state);
  this->publish_state(state);
}

}  // namespace gree
}  // namespace esphome
