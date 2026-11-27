#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::sensor {

/// on_value_range: runs fire() when the state enters the range; the in-range state is saved.
class ValueRangeBase : public FiringComponent<float> {
 public:
  /// range is generated: it returns the bounds for the state x.
  ValueRangeBase(Sensor *parent, ValueRange (*range)(float x)) : parent_(parent), range_(range) {}

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
      this->fire(state);
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
