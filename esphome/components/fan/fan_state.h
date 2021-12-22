#pragma once

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include "esphome/core/log.h"
#include "fan_traits.h"

namespace esphome {
namespace fan {

/// Simple enum to represent the speed of a fan. - DEPRECATED - Will be deleted soon
enum ESPDEPRECATED("FanSpeed is deprecated.", "2021.9") FanSpeed {
  FAN_SPEED_LOW = 0,     ///< The fan is running on low speed.
  FAN_SPEED_MEDIUM = 1,  ///< The fan is running on medium speed.
  FAN_SPEED_HIGH = 2     ///< The fan is running on high/full speed.
};

/// Simple enum to represent the direction of a fan
enum FanDirection { FAN_DIRECTION_FORWARD = 0, FAN_DIRECTION_REVERSE = 1 };

class FanState;

class FanStateCall {
 public:
  explicit FanStateCall(FanState *state) : state_(state) {}

  FanStateCall &set_state(bool binary_state) {
    this->binary_state_ = binary_state;
    return *this;
  }
  FanStateCall &set_state(optional<bool> binary_state) {
    this->binary_state_ = binary_state;
    return *this;
  }
  FanStateCall &set_oscillating(bool oscillating) {
    this->oscillating_ = oscillating;
    return *this;
  }
  FanStateCall &set_oscillating(optional<bool> oscillating) {
    this->oscillating_ = oscillating;
    return *this;
  }
  FanStateCall &set_speed(int speed) {
    this->speed_ = speed;
    return *this;
  }
  ESPDEPRECATED("set_speed() with string argument is deprecated, use integer argument instead.", "2021.9")
  FanStateCall &set_speed(const char *legacy_speed);
  FanStateCall &set_direction(FanDirection direction) {
    this->direction_ = direction;
    return *this;
  }
  FanStateCall &set_direction(optional<FanDirection> direction) {
    this->direction_ = direction;
    return *this;
  }

  void perform() const;

 protected:
  FanState *const state_;
  optional<bool> binary_state_;
  optional<bool> oscillating_;
  optional<int> speed_;
  optional<FanDirection> direction_{};
};

class FanState : public EntityBase, public Component {
 public:
  FanState() = default;
  /// Construct the fan state with name.
  explicit FanState(const std::string &name);

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
  /// The current fan speed level
  int speed{};
  /// The current direction of the fan
  FanDirection direction{FAN_DIRECTION_FORWARD};

  FanStateCall turn_on();
  FanStateCall turn_off();
  FanStateCall toggle();
  FanStateCall make_call();

  void setup() override;
  float get_setup_priority() const override;

 protected:
  friend FanStateCall;

  uint32_t hash_base() override;

  FanTraits traits_{};
  CallbackManager<void()> state_callback_{};
  ESPPreferenceObject rtc_;
};

}  // namespace fan
}  // namespace esphome
