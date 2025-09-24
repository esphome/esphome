#include "copy_number.h"
#include "esphome/core/log.h"

namespace esphome {
namespace copy {

static const char *const TAG = "copy.number";

void CopyNumber::setup() {
  source_->add_on_state_callback([this](float value) { this->publish_state(value); });

  traits.set_min_value(source_->traits.get_min_value());
  traits.set_max_value(source_->traits.get_max_value());
  traits.set_step(source_->traits.get_step());

  if (source_->has_state())
    this->publish_state(source_->state);
}

void CopyNumber::dump_config() { LOG_NUMBER("", "Copy Number", this); }

void CopyNumber::control(float value) {
  auto call2 = source_->make_call();
  call2.set_value(value);
  call2.perform();
}

}  // namespace copy
}  // namespace esphome
