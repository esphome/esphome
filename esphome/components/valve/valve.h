#pragma once

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include "valve_traits.h"

namespace esphome {
namespace valve {

const extern float VALVE_OPEN;
const extern float VALVE_CLOSED;

#define LOG_VALVE(prefix, type, obj) \
  if ((obj) != nullptr) { \
    ESP_LOGCONFIG(TAG, "%s%s '%s'", prefix, LOG_STR_LITERAL(type), (obj)->get_name().c_str()); \
    auto traits_ = (obj)->get_traits(); \
    if (traits_.get_is_assumed_state()) { \
      ESP_LOGCONFIG(TAG, "%s  Assumed State: YES", prefix); \
    } \
    if (!(obj)->get_device_class().empty()) { \
      ESP_LOGCONFIG(TAG, "%s  Device Class: '%s'", prefix, (obj)->get_device_class().c_str()); \
    } \
  }

class Valve;

class ValveCall {
 public:
  ValveCall(Valve *parent);

  /// Set the command as a string, "STOP", "OPEN", "CLOSE", "TOGGLE".
  ValveCall &set_command(const char *command);
  /// Set the command to open the valve.
  ValveCall &set_command_open();
  /// Set the command to close the valve.
  ValveCall &set_command_close();
  /// Set the command to stop the valve.
  ValveCall &set_command_stop();
  /// Set the command to toggle the valve.
  ValveCall &set_command_toggle();
  /// Set the call to a certain target position.
  ValveCall &set_position(float position);
  /// Set whether this valve call should stop the valve.
  ValveCall &set_stop(bool stop);

  /// Perform the valve call.
  void perform();

  const optional<float> &get_position() const;
  bool get_stop() const;
  const optional<bool> &get_toggle() const;

 protected:
  void validate_();

  Valve *parent_;
  bool stop_{false};
  optional<float> position_{};
  optional<bool> toggle_{};
};

/// Struct used to store the restored state of a valve
struct ValveRestoreState {
  float position;

  /// Convert this struct to a valve call that can be performed.
  ValveCall to_call(Valve *valve);
  /// Apply these settings to the valve
  void apply(Valve *valve);
} __attribute__((packed));

/// Enum encoding the current operation of a valve.
enum ValveOperation : uint8_t {
  /// The valve is currently idle (not moving)
  VALVE_OPERATION_IDLE = 0,
  /// The valve is currently opening.
  VALVE_OPERATION_OPENING,
  /// The valve is currently closing.
  VALVE_OPERATION_CLOSING,
};

const char *valve_operation_to_str(ValveOperation op);

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
class Valve : public EntityBase, public EntityBase_DeviceClass {
 public:
  explicit Valve();

  /// The current operation of the valve (idle, opening, closing).
  ValveOperation current_operation{VALVE_OPERATION_IDLE};
  /** The position of the valve from 0.0 (fully closed) to 1.0 (fully open).
   *
   * For binary valves this is always equals to 0.0 or 1.0 (see also VALVE_OPEN and
   * VALVE_CLOSED constants).
   */
  float position;

  /// Construct a new valve call used to control the valve.
  ValveCall make_call();

  void add_on_state_callback(std::function<void()> &&f);

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

 protected:
  friend ValveCall;

  virtual void control(const ValveCall &call) = 0;

  optional<ValveRestoreState> restore_state_();

  CallbackManager<void()> state_callback_{};

  ESPPreferenceObject rtc_;
};

}  // namespace valve
}  // namespace esphome
