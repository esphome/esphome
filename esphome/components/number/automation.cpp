#include "automation.h"
#include "esphome/core/log.h"

namespace esphome::number {

ESPHOME_LOG_TAG(TAG, "number.automation");

union convert {
  float from;
  uint32_t to;
};

void ValueRangeTrigger::setup() {
  convert hash = {.from = this->range_(0.0f).width()};
  this->rtc_ = this->parent_->make_entity_preference<bool>(hash.to);
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

  bool in_range = this->range_(state).contains(state, this->previous_in_range_);

  if (in_range != this->previous_in_range_ && in_range) {
    this->trigger(state);
  }

  this->previous_in_range_ = in_range;
  this->rtc_.save(&in_range);
}

}  // namespace esphome::number
