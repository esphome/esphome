#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include "fan_traits.h"

namespace esphome {
namespace fan {

/// Simple enum to represent the speed of a fan.
enum FanSpeed {
  FAN_SPEED_LOW = 0,     ///< The fan is running on low speed.
  FAN_SPEED_MEDIUM = 1,  ///< The fan is running on medium speed.
  FAN_SPEED_HIGH = 2     ///< The fan is running on high/full speed.
};

/// Simple enum to represent the direction of a fan
enum FanDirection { FAN_DIRECTION_FORWARD = 0, FAN_DIRECTION_REVERSE = 1 };

class Fan;

class FanCall {
 public:
  explicit FanCall(Fan *state) : state_(state) {}

  FanCall &set_state(bool binary_state) {
    this->binary_state_ = binary_state;
    return *this;
  }
  FanCall &set_state(optional<bool> binary_state) {
    this->binary_state_ = binary_state;
    return *this;
  }
  FanCall &set_oscillating(bool oscillating) {
    this->oscillating_ = oscillating;
    return *this;
  }
  FanCall &set_oscillating(optional<bool> oscillating) {
    this->oscillating_ = oscillating;
    return *this;
  }
  FanCall &set_speed(FanSpeed speed) {
    this->speed_ = speed;
    return *this;
  }
  FanCall &set_speed(optional<FanSpeed> speed) {
    this->speed_ = speed;
    return *this;
  }
  FanCall &set_speed(const char *speed);
  FanCall &set_direction(FanDirection direction) {
    this->direction_ = direction;
    return *this;
  }
  FanCall &set_direction(optional<FanDirection> direction) {
    this->direction_ = direction;
    return *this;
  }

  void perform() const;

 protected:
  Fan *const state_;
  optional<bool> binary_state_;
  optional<bool> oscillating_{};
  optional<FanSpeed> speed_{};
  optional<FanDirection> direction_{};
};

class Fan : public Nameable, public Component {
 public:
  Fan() = default;
  /// Construct the fan state with name.
  explicit Fan(const std::string &name);

  /// Register a callback that will be called each time the state changes.
  void add_on_state_callback(std::function<void()> &&callback);

  /// Get the traits of this fan (i.e. what features it supports).
  const FanTraits &get_traits() const;
  /// Set the traits of this fan (i.e. what features it supports).
  void set_traits(const FanTraits &traits);

  /// The current ON/OFF state of the fan.
  bool state{false};
  /// The current oscillation state of the fan.
  bool oscillating{false};
  /// The current fan speed.
  FanSpeed speed{FAN_SPEED_HIGH};
  /// The current direction of the fan
  FanDirection direction{FAN_DIRECTION_FORWARD};

  FanCall turn_on();
  FanCall turn_off();
  FanCall toggle();
  FanCall make_call();

  void setup() override;
  float get_setup_priority() const override;

  void publish_state();

 protected:
  friend FanCall;

  uint32_t hash_base() override;

  FanTraits traits_{};
  CallbackManager<void()> state_callback_{};
  ESPPreferenceObject rtc_;
};

}  // namespace fan
}  // namespace esphome
