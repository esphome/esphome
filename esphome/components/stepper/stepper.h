#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/stepper/stepper.h"

namespace esphome {
namespace stepper {

#define LOG_STEPPER(this) \
  ESP_LOGCONFIG(TAG, "  Acceleration: %.0f steps/s^2", this->acceleration_); \
  ESP_LOGCONFIG(TAG, "  Deceleration: %.0f steps/s^2", this->deceleration_); \
  ESP_LOGCONFIG(TAG, "  Max Speed: %.0f steps/s", this->max_speed_);

class Stepper {
 public:
  void set_target(int32_t steps) { this->target_position = steps; }
  void report_position(int32_t steps) { this->current_position = steps; }
  void set_acceleration(float acceleration) { this->acceleration_ = acceleration; }
  void set_deceleration(float deceleration) { this->deceleration_ = deceleration; }
  void set_max_speed(float max_speed) { this->max_speed_ = max_speed; }
  virtual void on_update_speed() {}
  bool has_reached_target() { return this->current_position == this->target_position; }

  int32_t current_position{0};
  int32_t target_position{0};

 protected:
  void calculate_speed_(uint32_t now);
  int32_t should_step_();

  float acceleration_{1e6f};
  float deceleration_{1e6f};
  float current_speed_{0.0f};
  float max_speed_{1e6f};
  uint32_t last_calculation_{0};
  uint32_t last_step_{0};
};

template<typename... Ts> class SetTargetAction : public Action<Ts...> {
 public:
  explicit SetTargetAction(Stepper *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(int32_t, target)

  void play(Ts... x) override { this->parent_->set_target(this->target_.value(x...)); }

 protected:
  Stepper *parent_;
};

template<typename... Ts> class ReportPositionAction : public Action<Ts...> {
 public:
  explicit ReportPositionAction(Stepper *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(int32_t, position)

  void play(Ts... x) override { this->parent_->report_position(this->position_.value(x...)); }

 protected:
  Stepper *parent_;
};

template<typename... Ts> class SetSpeedAction : public Action<Ts...> {
 public:
  explicit SetSpeedAction(Stepper *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(float, speed);

  void play(Ts... x) override {
    float speed = this->speed_.value(x...);
    this->parent_->set_max_speed(speed);
    this->parent_->on_update_speed();
  }

 protected:
  Stepper *parent_;
};

template<typename... Ts> class SetAccelerationAction : public Action<Ts...> {
 public:
  explicit SetAccelerationAction(Stepper *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(float, acceleration);

  void play(Ts... x) override {
    float acceleration = this->acceleration_.value(x...);
    this->parent_->set_acceleration(acceleration);
  }

 protected:
  Stepper *parent_;
};

template<typename... Ts> class SetDecelerationAction : public Action<Ts...> {
 public:
  explicit SetDecelerationAction(Stepper *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(float, deceleration);

  void play(Ts... x) override {
    float deceleration = this->deceleration_.value(x...);
    this->parent_->set_deceleration(deceleration);
  }

 protected:
  Stepper *parent_;
};

}  // namespace stepper
}  // namespace esphome
