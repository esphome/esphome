#include "copy_number.h"
#include "esphome/core/log.h"

namespace esphome::copy {

ESPHOME_LOG_TAG(TAG, "copy.number");

void CopyNumber::setup() {
  this->source_number_->add_on_state_callback([this](float value) { this->publish_state(value); });

  traits.set_min_value(this->source_number_->traits.get_min_value());
  traits.set_max_value(this->source_number_->traits.get_max_value());
  traits.set_step(this->source_number_->traits.get_step());

  if (this->source_number_->has_state())
    this->publish_state(this->source_number_->state);
}

void CopyNumber::dump_config() { LOG_NUMBER("", "Copy Number", this); }

void CopyNumber::control(float value) {
  auto call2 = this->source_number_->make_call();
  call2.set_value(value);
  call2.perform();
}

}  // namespace esphome::copy
