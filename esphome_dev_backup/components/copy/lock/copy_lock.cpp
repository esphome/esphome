#include "copy_lock.h"
#include "esphome/core/log.h"

namespace esphome {
namespace copy {

static const char *const TAG = "copy.lock";

void CopyLock::setup() {
  source_->add_on_state_callback([this]() { this->publish_state(source_->state); });

  traits.set_assumed_state(source_->traits.get_assumed_state());
  traits.set_requires_code(source_->traits.get_requires_code());
  traits.set_supported_states(source_->traits.get_supported_states());
  traits.set_supports_open(source_->traits.get_supports_open());

  this->publish_state(source_->state);
}

void CopyLock::dump_config() { LOG_LOCK("", "Copy Lock", this); }

void CopyLock::control(const lock::LockCall &call) {
  auto call2 = source_->make_call();
  call2.set_state(call.get_state());
  call2.perform();
}

}  // namespace copy
}  // namespace esphome
