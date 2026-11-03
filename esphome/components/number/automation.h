#pragma once

#include "esphome/components/number/number.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"

namespace esphome::number {

class ValueRangeTrigger final : public Trigger<float>, public Component {
 public:
  explicit ValueRangeTrigger(Number *parent) : parent_(parent) {}

  template<typename V> void set_min(V min) { this->min_ = min; }
  template<typename V> void set_max(V max) { this->max_ = max; }

  void setup() override;
  float get_setup_priority() const override;

 protected:
  void on_state_(float state);

  Number *parent_;
  ESPPreferenceObject rtc_;
  bool previous_in_range_{false};
  TemplatableFn<float, float> min_{[](float) -> float { return NAN; }};
  TemplatableFn<float, float> max_{[](float) -> float { return NAN; }};
};

}  // namespace esphome::number
