#pragma once

#include "esphome/components/number/number.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"

namespace esphome {
namespace number {

class NumberStateTrigger : public Trigger<float> {
 public:
  explicit NumberStateTrigger(Number *parent) {
    parent->add_on_state_callback([this](float value) { this->trigger(value); });
  }
};

template<typename... Ts> class NumberSetAction : public Action<Ts...> {
 public:
  NumberSetAction(Number *number) : number_(number) {}
  TEMPLATABLE_VALUE(float, value)

  void play(Ts... x) override {
    auto call = this->number_->make_call();
    call.set_value(this->value_.value(x...));
    call.perform();
  }

 protected:
  Number *number_;
};

class ValueRangeTrigger : public Trigger<float>, public Component {
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
  TemplatableValue<float, float> min_{NAN};
  TemplatableValue<float, float> max_{NAN};
};

template<typename... Ts> class NumberInRangeCondition : public Condition<Ts...> {
 public:
  NumberInRangeCondition(Number *parent) : parent_(parent) {}

  void set_min(float min) { this->min_ = min; }
  void set_max(float max) { this->max_ = max; }
  bool check(Ts... x) override {
    const float state = this->parent_->state;
    if (std::isnan(this->min_)) {
      return state <= this->max_;
    } else if (std::isnan(this->max_)) {
      return state >= this->min_;
    } else {
      return this->min_ <= state && state <= this->max_;
    }
  }

 protected:
  Number *parent_;
  float min_{NAN};
  float max_{NAN};
};

}  // namespace number
}  // namespace esphome
