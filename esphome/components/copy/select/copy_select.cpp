#include "copy_select.h"
#include "esphome/core/log.h"

namespace esphome::copy {

static const char *const TAG = "copy.select";

void CopySelect::setup() {
  source_->add_on_state_callback([this](size_t index) { this->publish_state(index); });

  traits.set_options(source_->traits.get_options());

  auto idx = this->source_->active_index();
  if (idx.has_value())
    this->publish_state(*idx);
}

void CopySelect::dump_config() { LOG_SELECT("", "Copy Select", this); }

void CopySelect::control(size_t index) {
  auto call = source_->make_call();
  call.set_index(index);
  call.perform();
}

}  // namespace esphome::copy
