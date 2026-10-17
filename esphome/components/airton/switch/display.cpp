#include "display.h"

namespace esphome::airton {

// The parent sends the state to the unit and publishes it back to this switch
void DisplaySwitch::write_state(bool state) { this->parent_->set_display_state(state, true); }

}  // namespace esphome::airton
