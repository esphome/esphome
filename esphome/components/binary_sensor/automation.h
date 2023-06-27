#pragma once

#include <utility>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/hal.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace binary_sensor {

struct MultiClickTriggerEvent {
  bool state;
  uint32_t min_length;
  uint32_t max_length;
};

class PressTrigger : public Trigger<> {
 public:
  explicit PressTrigger(BinarySensor *parent) {
    parent->add_on_state_callback([this](bool state) {
      if (state)
        this->trigger();
    });
  }
};

class ReleaseTrigger : public Trigger<> {
 public:
  explicit ReleaseTrigger(BinarySensor *parent) {
    parent->add_on_state_callback([this](bool state) {
      if (!state)
        this->trigger();
    });
  }
};

bool match_interval(uint32_t min_length, uint32_t max_length, uint32_t length);

class ClickTrigger : public Trigger<> {
 public:
  explicit ClickTrigger(BinarySensor *parent, uint32_t min_length, uint32_t max_length)
      : min_length_(min_length), max_length_(max_length) {
    parent->add_on_state_callback([this](bool state) {
      if (state) {
        this->start_time_ = millis();
      } else {
        const uint32_t length = millis() - this->start_time_;
        if (match_interval(this->min_length_, this->max_length_, length))
          this->trigger();
      }
    });
  }

 protected:
  uint32_t start_time_{0};  /// The millis() time when the click started.
  uint32_t min_length_;     /// Minimum length of click. 0 means no minimum.
  uint32_t max_length_;     /// Maximum length of click. 0 means no maximum.
};

class DoubleClickTrigger : public Trigger<> {
 public:
  explicit DoubleClickTrigger(BinarySensor *parent, uint32_t min_length, uint32_t max_length)
      : min_length_(min_length), max_length_(max_length) {
    parent->add_on_state_callback([this](bool state) {
      const uint32_t now = millis();

      if (state && this->start_time_ != 0 && this->end_time_ != 0) {
        if (match_interval(this->min_length_, this->max_length_, this->end_time_ - this->start_time_) &&
            match_interval(this->min_length_, this->max_length_, now - this->end_time_)) {
          this->trigger();
          this->start_time_ = 0;
          this->end_time_ = 0;
          return;
        }
      }

      this->start_time_ = this->end_time_;
      this->end_time_ = now;
    });
  }

 protected:
  uint32_t start_time_{0};
  uint32_t end_time_{0};
  uint32_t min_length_;  /// Minimum length of click. 0 means no minimum.
  uint32_t max_length_;  /// Maximum length of click. 0 means no maximum.
};

class MultiClickTrigger : public Trigger<>, public Component {
 public:
  explicit MultiClickTrigger(BinarySensor *parent, std::vector<MultiClickTriggerEvent> timing)
      : parent_(parent), timing_(std::move(timing)) {}

  void setup() override {
    this->last_state_ = this->parent_->state;
    auto f = std::bind(&MultiClickTrigger::on_state_, this, std::placeholders::_1);
    this->parent_->add_on_state_callback(f);
  }

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void set_invalid_cooldown(uint32_t invalid_cooldown) { this->invalid_cooldown_ = invalid_cooldown; }

 protected:
  void on_state_(bool state);
  void schedule_cooldown_();
  void schedule_is_valid_(uint32_t min_length);
  void schedule_is_not_valid_(uint32_t max_length);
  void trigger_();

  BinarySensor *parent_;
  std::vector<MultiClickTriggerEvent> timing_;
  uint32_t invalid_cooldown_{1000};
  optional<size_t> at_index_{};
  bool last_state_{false};
  bool is_in_cooldown_{false};
  bool is_valid_{false};
};

class StateTrigger : public Trigger<bool> {
 public:
  explicit StateTrigger(BinarySensor *parent) {
    parent->add_on_state_callback([this](bool state) { this->trigger(state); });
  }
};

template<typename... Ts> class BinarySensorCondition : public Condition<Ts...> {
 public:
  BinarySensorCondition(BinarySensor *parent, bool state) : parent_(parent), state_(state) {}
  bool check(Ts... x) override { return this->parent_->state == this->state_; }

 protected:
  BinarySensor *parent_;
  bool state_;
};

template<typename... Ts> class BinarySensorPublishAction : public Action<Ts...> {
 public:
  explicit BinarySensorPublishAction(BinarySensor *sensor) : sensor_(sensor) {}
  TEMPLATABLE_VALUE(bool, state)

  void play(Ts... x) override {
    auto val = this->state_.value(x...);
    this->sensor_->publish_state(val);
  }

 protected:
  BinarySensor *sensor_;
};

}  // namespace binary_sensor
}  // namespace esphome
