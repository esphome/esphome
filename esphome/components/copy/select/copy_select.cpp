#include "copy_select.h"
#include "esphome/core/log.h"

namespace esphome {
namespace copy {

static const char *const TAG = "copy.select";

void CopySelect::setup() {
  source_->add_on_state_callback([this](const std::string &value, size_t index) { this->publish_state(value); });

  traits.set_options(source_->traits.get_options());

  if (source_->has_state())
    this->publish_state(source_->state);
}

void CopySelect::dump_config() { LOG_SELECT("", "Copy Select", this); }

void CopySelect::control(const std::string &value) {
  auto call = source_->make_call();
  call.set_option(value);
  call.perform();
}

}  // namespace copy
}  // namespace esphome
