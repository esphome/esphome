#include "increase.h"

namespace esphome::vornado_ir {

static const char *const TAG = "vornado_ir.increase_button";

void IncreaseButton::press_action() { this->parent_->send_increase(); }

}  // namespace esphome::vornado_ir
