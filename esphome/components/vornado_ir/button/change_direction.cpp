#include "change_direction.h"

namespace esphome::vornado_ir {

static const char *const TAG = "vornado_ir.change_direction_button";

void ChangeDirectionButton::press_action() { this->parent_->send_change_direction(); }

}  // namespace esphome::vornado_ir
