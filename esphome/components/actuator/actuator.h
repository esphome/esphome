#pragma once

#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include "iactuator.h"

namespace esphome::actuator {

static constexpr float ACTUATOR_OPEN = 1.0f;
static constexpr float ACTUATOR_CLOSED = 0.0f;

/// Enum encoding the current operation of an actuator.
enum ActuatorOperation : uint8_t {
  /// The actuator is currently idle (not moving)
  ACTUATOR_OPERATION_IDLE = 0,
  /// The actuator is currently opening.
  ACTUATOR_OPERATION_OPENING,
  /// The actuator is currently closing.
  ACTUATOR_OPERATION_CLOSING,
};

const LogString *actuator_operation_to_str(ActuatorOperation op);

class ActuatorBase;

// Inheritance: ActuatorCallBase — base call class for actuator commands
class ActuatorCallBase {
 public:
  explicit ActuatorCallBase(ActuatorBase *parent) : parent_(parent) {}

  ActuatorCallBase &set_command_open() {
    this->position_ = ACTUATOR_OPEN;
    return *this;
  }
  ActuatorCallBase &set_command_close() {
    this->position_ = ACTUATOR_CLOSED;
    return *this;
  }
  ActuatorCallBase &set_command_stop() {
    this->stop_ = true;
    return *this;
  }
  ActuatorCallBase &set_command_toggle() {
    this->toggle_ = true;
    return *this;
  }
  ActuatorCallBase &set_position(float position) {
    this->position_ = position;
    return *this;
  }
  ActuatorCallBase &set_stop(bool stop) {
    this->stop_ = stop;
    return *this;
  }

  const optional<float> &get_position() const { return this->position_; }
  bool get_stop() const { return this->stop_; }
  const optional<bool> &get_toggle() const { return this->toggle_; }

 protected:
  /// Apply a command given as a string ("OPEN", "CLOSE", "STOP", "TOGGLE"). Returns false if not recognized.
  bool set_command_(const char *command);

  ActuatorBase *parent_;
  bool stop_{false};
  optional<float> position_{};
  optional<bool> toggle_{};
};

// Inheritance: ActuatorBase -> EntityBase
class ActuatorBase : public EntityBase {
 public:
  /** The position of the actuator from 0.0 (fully closed) to 1.0 (fully open).
   *
   * For binary actuators this is always equal to 0.0 or 1.0 (see also ACTUATOR_OPEN and
   * ACTUATOR_CLOSED constants).
   */
  float position{ACTUATOR_CLOSED};
  /// The current operation of the actuator (idle, opening, closing).
  ActuatorOperation current_operation{ACTUATOR_OPERATION_IDLE};

  /// Helper method to check if the actuator is fully open. Equivalent to comparing .position against 1.0
  bool is_fully_open() const;
  /// Helper method to check if the actuator is fully closed. Equivalent to comparing .position against 0.0
  bool is_fully_closed() const;

  template<typename F> void add_on_state_callback(F &&f) { this->state_callback_.add(std::forward<F>(f)); }

 protected:
  template<typename T> optional<T> restore_state_() {
    this->rtc_ = this->make_entity_preference<T>();
    T recovered{};
    if (!this->rtc_.load(&recovered))
      return {};
    return recovered;
  }

  LazyCallbackManager<void()> state_callback_{};
  ESPPreferenceObject rtc_;
};

}  // namespace esphome::actuator
