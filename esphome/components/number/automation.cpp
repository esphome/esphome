#include "automation.h"
#include "esphome/core/log.h"

namespace esphome {
namespace number {

static const char *const TAG = "number.automation";

void ValueRangeTrigger::setup() {
  this->rtc_ = global_preferences.make_preference<bool>(this->parent_->get_object_id_hash());
  bool initial_state;
  if (this->rtc_.load(&initial_state)) {
    this->previous_in_range_ = initial_state;
  }

  this->parent_->add_on_state_callback([this](float state) { this->on_state_(state); });
}
float ValueRangeTrigger::get_setup_priority() const { return setup_priority::HARDWARE; }

void ValueRangeTrigger::on_state_(float state) {
  if (isnan(state))
    return;

  float local_min = this->min_.value(state);
  float local_max = this->max_.value(state);

  bool in_range;
  if (isnan(local_min) && isnan(local_max)) {
    in_range = this->previous_in_range_;
  } else if (isnan(local_min)) {
    in_range = state <= local_max;
  } else if (isnan(local_max)) {
    in_range = state >= local_min;
  } else {
    in_range = local_min <= state && state <= local_max;
  }

  if (in_range != this->previous_in_range_ && in_range) {
    this->trigger(state);
  }

  this->previous_in_range_ = in_range;
  this->rtc_.save(&in_range);
}

}  // namespace number
}  // namespace esphome
