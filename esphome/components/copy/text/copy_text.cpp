#include "copy_text.h"
#include "esphome/core/log.h"

namespace esphome {
namespace copy {

static const char *const TAG = "copy.text";

void CopyText::setup() {
  source_->add_on_state_callback([this](const std::string &value) { this->publish_state(value); });

  if (source_->has_state())
    this->publish_state(source_->state);
}

void CopyText::dump_config() { LOG_TEXT("", "Copy Text", this); }

void CopyText::control(const std::string &value) {
  auto call2 = source_->make_call();
  call2.set_value(value);
  call2.perform();
}

}  // namespace copy
}  // namespace esphome
