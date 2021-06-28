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

template<typename... Ts> class NumberPublishAction : public Action<Ts...> {
 public:
  NumberPublishAction(Number *number) : number_(number) {}
  TEMPLATABLE_VALUE(float, state)

  void play(Ts... x) override { this->number_->publish_state(this->state_.value(x...)); }

 protected:
  Number *number_;
};

class ValueRangeTrigger : public Trigger<float>, public Component {
 public:
  explicit ValueRangeTrigger(Number *parent) : parent_(parent) {}

  template<typename V> void set_min(V min) { this->min_ = min; }
  template<typename V> void set_max(V max) { this->max_ = max; }

  void setup() override {
    this->rtc_ = global_preferences.make_preference<bool>(this->parent_->get_object_id_hash());
    bool initial_state;
    if (this->rtc_.load(&initial_state)) {
      this->previous_in_range_ = initial_state;
    }

    this->parent_->add_on_state_callback([this](float state) { this->on_state_(state); });
  }
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  void on_state_(float state) {
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
    if (isnan(this->min_)) {
      return state <= this->max_;
    } else if (isnan(this->max_)) {
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
