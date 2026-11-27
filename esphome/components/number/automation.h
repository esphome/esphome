#pragma once

#include "esphome/components/number/number.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"

namespace esphome::number {

/// on_value_range: runs fire() when the state enters the range; the in-range state is saved.
class ValueRangeBase : public FiringComponent<float> {
 public:
  /// range is generated: it returns the bounds for the state x.
  ValueRangeBase(Number *parent, ValueRange (*range)(float x)) : parent_(parent), range_(range) {}

  void setup() override;
  float get_setup_priority() const override;

 protected:
  void on_state_(float state);

  Number *parent_;
  ESPPreferenceObject rtc_;
  ValueRange (*range_)(float x);
  bool previous_in_range_{false};
};

}  // namespace esphome::number
