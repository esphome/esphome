#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace sensor {

class SensorStateTrigger : public Trigger<float> {
 public:
  explicit SensorStateTrigger(Sensor *parent) {
    parent->add_on_state_callback([this](float value) { this->trigger(value); });
  }
};

class SensorRawStateTrigger : public Trigger<float> {
 public:
  explicit SensorRawStateTrigger(Sensor *parent) {
    parent->add_on_raw_state_callback([this](float value) { this->trigger(value); });
  }
};

template<typename... Ts> class SensorPublishAction : public Action<Ts...> {
 public:
  SensorPublishAction(Sensor *sensor) : sensor_(sensor) {}
  TEMPLATABLE_VALUE(float, state)

  void play(Ts... x) override { this->sensor_->publish_state(this->state_.value(x...)); }

 protected:
  Sensor *sensor_;
};

class ValueRangeTrigger : public Trigger<float>, public Component {
 public:
  explicit ValueRangeTrigger(Sensor *parent) : parent_(parent) {}

  template<typename V> void set_min(V min) { this->min_ = min; }
  template<typename V> void set_max(V max) { this->max_ = max; }

  void setup() override {
    this->rtc_ = global_preferences->make_preference<bool>(this->parent_->get_object_id_hash());
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

  Sensor *parent_;
  ESPPreferenceObject rtc_;
  bool previous_in_range_{false};
  TemplatableValue<float, float> min_{NAN};
  TemplatableValue<float, float> max_{NAN};
};

template<typename... Ts> class SensorInRangeCondition : public Condition<Ts...> {
 public:
  SensorInRangeCondition(Sensor *parent) : parent_(parent) {}

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
  Sensor *parent_;
  float min_{NAN};
  float max_{NAN};
};

}  // namespace sensor
}  // namespace esphome
