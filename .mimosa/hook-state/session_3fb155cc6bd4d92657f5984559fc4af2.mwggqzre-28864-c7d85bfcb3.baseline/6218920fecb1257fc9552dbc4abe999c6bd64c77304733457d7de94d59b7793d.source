#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::sensor {

class ValueRangeTrigger final : public Trigger<float>, public Component {
 public:
  /// range is generated: it returns the bounds for the state x.
  ValueRangeTrigger(Sensor *parent, ValueRange (*range)(float x)) : parent_(parent), range_(range) {}

  void setup() override {
    this->rtc_ = this->parent_->make_entity_preference<bool>();
    bool initial_state;
    if (this->rtc_.load(&initial_state)) {
      this->previous_in_range_ = initial_state;
    }

    this->parent_->add_on_state_callback([this](float state) { this->on_state_(state); });
  }
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  void on_state_(float state) {
    if (std::isnan(state))
      return;

    bool in_range = this->range_(state).contains(state, this->previous_in_range_);
    if (in_range != this->previous_in_range_ && in_range) {
      this->trigger(state);
    }

    this->previous_in_range_ = in_range;
    this->rtc_.save(&in_range);
  }

  Sensor *parent_;
  ESPPreferenceObject rtc_;
  ValueRange (*range_)(float x);
  bool previous_in_range_{false};
};

}  // namespace esphome::sensor
