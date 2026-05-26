#pragma once

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include "esphome/components/actuator/actuator.h"

#include "valve_traits.h"

namespace esphome::valve {

// Backward-compat aliases.
using ValveOperation = actuator::ActuatorOperation;
constexpr actuator::ActuatorOperation VALVE_OPERATION_IDLE = actuator::ACTUATOR_OPERATION_IDLE;
constexpr actuator::ActuatorOperation VALVE_OPERATION_OPENING = actuator::ACTUATOR_OPERATION_OPENING;
constexpr actuator::ActuatorOperation VALVE_OPERATION_CLOSING = actuator::ACTUATOR_OPERATION_CLOSING;
static constexpr float VALVE_OPEN = actuator::ACTUATOR_OPEN;
static constexpr float VALVE_CLOSED = actuator::ACTUATOR_CLOSED;

#define LOG_VALVE(prefix, type, obj) \
  if ((obj) != nullptr) { \
    ESP_LOGCONFIG(TAG, "%s%s '%s'", prefix, LOG_STR_LITERAL(type), (obj)->get_name().c_str()); \
    auto traits_ = (obj)->get_traits(); \
    if (traits_.get_is_assumed_state()) { \
      ESP_LOGCONFIG(TAG, "%s  Assumed State: YES", prefix); \
    } \
    LOG_ENTITY_DEVICE_CLASS(TAG, prefix, *(obj)); \
  }

class Valve;

// Inheritance: ValveCall -> ActuatorCallBase
class ValveCall : public actuator::ActuatorCallBase {
 public:
  explicit ValveCall(Valve *parent);

  // Covariant wrappers — return ValveCall& for fluent chaining compatibility
  ValveCall &set_command(const char *command);
  ValveCall &set_command_open();
  ValveCall &set_command_close();
  ValveCall &set_command_stop();
  ValveCall &set_command_toggle();
  ValveCall &set_position(float position);
  ValveCall &set_stop(bool stop);

  void perform();

  const optional<float> &get_position() const { return this->position_; }
  bool get_stop() const { return this->stop_; }
  const optional<bool> &get_toggle() const { return this->toggle_; }

 protected:
  void validate() override;
};

/// Struct used to store the restored state of a valve
struct ValveRestoreState {
  float position;

  /// Convert this struct to a valve call that can be performed.
  ValveCall to_call(Valve *valve);
  /// Apply these settings to the valve
  void apply(Valve *valve);
} __attribute__((packed));

const LogString *valve_operation_to_str(ValveOperation op);

/** Base class for all valve devices.
 *
 * Valves currently have three properties:
 *  - position - The current position of the valve from 0.0 (fully closed) to 1.0 (fully open).
 *    For valves with only binary OPEN/CLOSED position this will always be either 0.0 or 1.0
 *  - current_operation - The operation the valve is currently performing, this can
 *    be one of IDLE, OPENING and CLOSING.
 *
 * For users: All valve operations must be performed over the .make_call() interface.
 * To command a valve, use .make_call() to create a call object, set all properties
 * you wish to set, and activate the command with .perform().
 * For reading out the current values of the valve, use the public .position, etc.
 * properties (these are read-only for users)
 *
 * For integrations: Integrations must implement two methods: control() and get_traits().
 * Control will be called with the arguments supplied by the user and should be used
 * to control all values of the valve. Also implement get_traits() to return what operations
 * the valve supports.
 */
// Inheritance: Valve -> ActuatorBase, IActuator
class Valve : public actuator::ActuatorBase, public actuator::IActuator {
 public:
  explicit Valve();

  /// Construct a new valve call used to control the valve.
  ValveCall make_call();

  using actuator::ActuatorBase::add_on_state_callback;

  /** Publish the current state of the valve.
   *
   * First set the .position, etc. values and then call this method
   * to publish the state of the valve.
   *
   * @param save Whether to save the updated values in RTC area.
   */
  void publish_state(bool save = true);

  virtual ValveTraits get_traits() = 0;

  /// Helper method to check if the valve is fully open. Equivalent to comparing .position against 1.0
  bool is_fully_open() const;
  /// Helper method to check if the valve is fully closed. Equivalent to comparing .position against 0.0
  bool is_fully_closed() const;

  // IActuator implementation
  float get_position() const override { return this->position; }
  void set_position(float p) override { this->position = p; }
  actuator::ActuatorOperation get_operation() const override { return this->current_operation; }
  void set_operation(actuator::ActuatorOperation op) override { this->current_operation = op; }
  void do_publish_state(bool save) override { this->publish_state(save); }
  optional<float> do_restore_state() override;
  const char *get_entity_name() const override { return this->get_name().c_str(); }

 protected:
  friend ValveCall;

  virtual void control(const ValveCall &call) = 0;

  // Bridge from ActuatorBase — safe cast because Valve::make_call() always constructs a ValveCall
  void control(const actuator::ActuatorCallBase &call) final { this->control(static_cast<const ValveCall &>(call)); }

  optional<ValveRestoreState> restore_state_() { return ActuatorBase::restore_state_<ValveRestoreState>(); }
};

}  // namespace esphome::valve
