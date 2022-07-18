#include "automation.h"
#include "esphome/core/log.h"

namespace esphome {
namespace number {

static const char *const TAG = "number.automation";

union convert {
  float from;
  uint32_t to;
};

void ValueRangeTrigger::setup() {
  float local_min = this->min_.value(0.0);
  float local_max = this->max_.value(0.0);
  convert hash = {.from = (local_max - local_min)};
  uint32_t myhash = hash.to ^ this->parent_->get_object_id_hash();
  this->rtc_ = global_preferences->make_preference<bool>(myhash);
  bool initial_state;
  if (this->rtc_.load(&initial_state)) {
    this->previous_in_range_ = initial_state;
  }

  this->parent_->add_on_state_callback([this](float state) { this->on_state_(state); });
}
float ValueRangeTrigger::get_setup_priority() const { return setup_priority::HARDWARE; }

void ValueRangeTrigger::on_state_(float state) {
  if (std::isnan(state))
    return;

  float local_min = this->min_.value(state);
  float local_max = this->max_.value(state);

  bool in_range;
  if (std::isnan(local_min) && std::isnan(local_max)) {
    in_range = this->previous_in_range_;
  } else if (std::isnan(local_min)) {
    in_range = state <= local_max;
  } else if (std::isnan(local_max)) {
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
