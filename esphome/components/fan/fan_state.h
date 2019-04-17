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

class FanState : public Nameable, public Component {
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
  /// The current fan speed.
  FanSpeed speed{FAN_SPEED_HIGH};

  class StateCall {
   public:
    explicit StateCall(FanState *state);

    FanState::StateCall &set_state(bool state);
    FanState::StateCall &set_state(optional<bool> state);
    FanState::StateCall &set_oscillating(bool oscillating);
    FanState::StateCall &set_oscillating(optional<bool> oscillating);
    FanState::StateCall &set_speed(FanSpeed speed);
    FanState::StateCall &set_speed(optional<FanSpeed> speed);
    FanState::StateCall &set_speed(const char *speed);

    void perform() const;

   protected:
    FanState *const state_;
    optional<bool> binary_state_;
    optional<bool> oscillating_{};
    optional<FanSpeed> speed_{};
  };

  FanState::StateCall turn_on();
  FanState::StateCall turn_off();
  FanState::StateCall toggle();
  FanState::StateCall make_call();

  void setup() override;
  float get_setup_priority() const override;

 protected:
  uint32_t hash_base() override;

  FanTraits traits_{};
  CallbackManager<void()> state_callback_{};
  ESPPreferenceObject rtc_;
};

}  // namespace fan
}  // namespace esphome
