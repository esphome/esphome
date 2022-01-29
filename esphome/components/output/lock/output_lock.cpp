#include "output_lock.h"
#include "esphome/core/log.h"

namespace esphome {
namespace output {

static const char *const TAG = "output.lock";

void OutputLock::dump_config() { LOG_LOCK("", "Output Lock", this); }

void OutputLock::control(const lock::LockCall &call) {
  auto state = *call.get_state();
  if (state == lock::LOCK_STATE_LOCKED) {
    this->output_->turn_on();
  } else if (state == lock::LOCK_STATE_UNLOCKED) {
    this->output_->turn_off();
  }
  this->publish_state(state);
}

}  // namespace output
}  // namespace esphome
