#include "copy_lock.h"
#include "esphome/core/log.h"

namespace esphome::copy {

ESPHOME_LOG_TAG(TAG, "copy.lock");

void CopyLock::setup() {
  source_->add_on_state_callback([this](lock::LockState state) { this->publish_state(state); });

  this->traits = this->source_->traits;

  this->publish_state(source_->state);
}

void CopyLock::dump_config() { LOG_LOCK("", "Copy Lock", this); }

void CopyLock::control(const lock::LockCall &call) {
  auto call2 = source_->make_call();
  call2.set_state(call.get_state());
  call2.perform();
}

void CopyLock::open_latch() { this->source_->open(); }

}  // namespace esphome::copy
