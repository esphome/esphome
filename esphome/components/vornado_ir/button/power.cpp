#include "power.h"

namespace esphome::vornado_ir {

static const char *const TAG = "vornado_ir.power_toggle_button";

void PowerButton::press_action() { this->parent_->send_power_toggle(); }

}  // namespace esphome::vornado_ir
