#pragma once

#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/optional.h"
#include "esphome/core/preferences.h"
#include "fan_traits.h"

namespace esphome {
namespace fan {

#define LOG_FAN(prefix, type, obj) \
  if ((obj) != nullptr) { \
    ESP_LOGCONFIG(TAG, "%s%s '%s'", prefix, LOG_STR_LITERAL(type), (obj)->get_name().c_str()); \
    (obj)->dump_traits_(TAG, prefix); \
  }

/// Simple enum to represent the direction of a fan.
enum class FanDirection { FORWARD = 0, REVERSE = 1 };

/// Restore mode of a fan.
enum class FanRestoreMode {
  NO_RESTORE,
  ALWAYS_OFF,
  ALWAYS_ON,
  RESTORE_DEFAULT_OFF,
  RESTORE_DEFAULT_ON,
  RESTORE_INVERTED_DEFAULT_OFF,
  RESTORE_INVERTED_DEFAULT_ON,
};

const LogString *fan_direction_to_string(FanDirection direction);

class Fan;

class FanCall {
 public:
  explicit FanCall(Fan &parent) : parent_(parent) {}

  FanCall &set_state(bool binary_state) {
    this->binary_state_ = binary_state;
    return *this;
  }
  FanCall &set_state(optional<bool> binary_state) {
    this->binary_state_ = binary_state;
    return *this;
  }
  optional<bool> get_state() const { return this->binary_state_; }
  FanCall &set_oscillating(bool oscillating) {
    this->oscillating_ = oscillating;
    return *this;
  }
  FanCall &set_oscillating(optional<bool> oscillating) {
    this->oscillating_ = oscillating;
    return *this;
  }
  optional<bool> get_oscillating() const { return this->oscillating_; }
  FanCall &set_speed(int speed) {
    this->speed_ = speed;
    return *this;
  }
  ESPDEPRECATED("set_speed() with string argument is deprecated, use integer argument instead.", "2021.9")
  FanCall &set_speed(const char *legacy_speed);
  optional<int> get_speed() const { return this->speed_; }
  FanCall &set_direction(FanDirection direction) {
    this->direction_ = direction;
    return *this;
  }
  FanCall &set_direction(optional<FanDirection> direction) {
    this->direction_ = direction;
    return *this;
  }
  optional<FanDirection> get_direction() const { return this->direction_; }

  void perform();

 protected:
  void validate_();

  Fan &parent_;
  optional<bool> binary_state_;
  optional<bool> oscillating_;
  optional<int> speed_;
  optional<FanDirection> direction_{};
};

struct FanRestoreState {
  bool state;
  int speed;
  bool oscillating;
  FanDirection direction;

  /// Convert this struct to a fan call that can be performed.
  FanCall to_call(Fan &fan);
  /// Apply these settings to the fan.
  void apply(Fan &fan);
} __attribute__((packed));

class Fan : public EntityBase {
 public:
  /// The current on/off state of the fan.
  bool state{false};
  /// The current oscillation state of the fan.
  bool oscillating{false};
  /// The current fan speed level
  int speed{0};
  /// The current direction of the fan
  FanDirection direction{FanDirection::FORWARD};

  FanCall turn_on();
  FanCall turn_off();
  FanCall toggle();
  FanCall make_call();

  /// Register a callback that will be called each time the state changes.
  void add_on_state_callback(std::function<void()> &&callback);

  void publish_state();

  virtual FanTraits get_traits() = 0;

  /// Set the restore mode of this fan.
  void set_restore_mode(FanRestoreMode restore_mode) { this->restore_mode_ = restore_mode; }

 protected:
  friend FanCall;

  virtual void control(const FanCall &call) = 0;

  optional<FanRestoreState> restore_state_();
  void save_state_();

  void dump_traits_(const char *tag, const char *prefix);

  CallbackManager<void()> state_callback_{};
  ESPPreferenceObject rtc_;
  FanRestoreMode restore_mode_;
};

}  // namespace fan
}  // namespace esphome
