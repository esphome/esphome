#include "output_lock.h"
#include "esphome/core/log.h"

namespace esphome::output {

static const char *const TAG = "output.lock";

void OutputLock::dump_config() { LOG_LOCK("", "Output Lock", this); }

void OutputLock::control(const lock::LockCall &call) {
  auto state_val = call.get_state();
  if (!state_val.has_value())
    return;
  auto state = *state_val;
  if (state == lock::LOCK_STATE_LOCKED) {
    this->output_->turn_on();
  } else if (state == lock::LOCK_STATE_UNLOCKED) {
    this->output_->turn_off();
  }
  this->publish_state(state);
}

}  // namespace esphome::output
