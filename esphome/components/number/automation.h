#pragma once

#include "esphome/components/number/number.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"

namespace esphome::number {

/// on_value_range: runs fire() when the state enters the range; the in-range state is saved.
class ValueRangeBase : public Component {
 public:
  /// range is generated: it returns the bounds for the state x.
  ValueRangeBase(Number *parent, ValueRange (*range)(float x)) : parent_(parent), range_(range) {}

  void setup() override;
  float get_setup_priority() const override;

 protected:
  virtual void fire(float x) = 0;
  void on_state_(float state);

  Number *parent_;
  ESPPreferenceObject rtc_;
  ValueRange (*range_)(float x);
  bool previous_in_range_{false};
};

/// Calls Fn with the state, for on_value_range.
template<auto Fn> class ValueRangeCallback final : public ValueRangeBase {
 public:
  using ValueRangeBase::ValueRangeBase;

 protected:
  void fire(float x) override { Fn(x); }
};

}  // namespace esphome::number
