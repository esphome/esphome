#include "output_lock.h"
#include "esphome/core/log.h"

namespace esphome {
namespace output {

static const char *const TAG = "output.lock";

void OutputLock::dump_config() { LOG_LOCK("", "Output Lock", this); }
void OutputLock::setup() {
  auto restored = this->get_initial_state();
  if (!restored.has_value())
    return;

  if (*restored) {
    this->lock();
  } else {
    this->unlock();
  }
}
void OutputLock::write_state(bool state) {
  if (state) {
    this->output_->lock();
  } else {
    this->output_->unlock();
  }
  this->publish_state(state);
}

}  // namespace output
}  // namespace esphome
