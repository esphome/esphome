#include "sleep.h"

namespace esphome::airton {

// The parent sends the state to the unit and publishes it back to this switch
void SleepSwitch::write_state(bool state) { this->parent_->set_sleep_mode_state(state, true); }

}  // namespace esphome::airton
