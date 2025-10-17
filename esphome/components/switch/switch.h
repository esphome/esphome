#pragma once

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace switch_ {

#define SUB_SWITCH(name) \
 protected: \
  switch_::Switch *name##_switch_{nullptr}; \
\
 public: \
  void set_##name##_switch(switch_::Switch *s) { this->name##_switch_ = s; }

// bit0: on/off. bit1: persistent. bit2: inverted. bit3: disabled
const int RESTORE_MODE_ON_MASK = 0x01;
const int RESTORE_MODE_PERSISTENT_MASK = 0x02;
const int RESTORE_MODE_INVERTED_MASK = 0x04;
const int RESTORE_MODE_DISABLED_MASK = 0x08;

enum SwitchRestoreMode : uint8_t {
  SWITCH_ALWAYS_OFF = !RESTORE_MODE_ON_MASK,
  SWITCH_ALWAYS_ON = RESTORE_MODE_ON_MASK,
  SWITCH_RESTORE_DEFAULT_OFF = RESTORE_MODE_PERSISTENT_MASK,
  SWITCH_RESTORE_DEFAULT_ON = RESTORE_MODE_PERSISTENT_MASK | RESTORE_MODE_ON_MASK,
  SWITCH_RESTORE_INVERTED_DEFAULT_OFF = RESTORE_MODE_PERSISTENT_MASK | RESTORE_MODE_INVERTED_MASK,
  SWITCH_RESTORE_INVERTED_DEFAULT_ON = RESTORE_MODE_PERSISTENT_MASK | RESTORE_MODE_INVERTED_MASK | RESTORE_MODE_ON_MASK,
  SWITCH_RESTORE_DISABLED = RESTORE_MODE_DISABLED_MASK,
};

/** Base class for all switches.
 *
 * A switch is basically just a combination of a binary sensor (for reporting switch values)
 * and a write_state method that writes a state to the hardware.
 */
class Switch : public EntityBase, public EntityBase_DeviceClass {
 public:
  explicit Switch();

  /** Publish a state to the front-end from the back-end.
   *
   * The input value is inverted if applicable. Then the internal value member is set and
   * finally the callbacks are called.
   *
   * @param state The new state.
   */
  void publish_state(bool state);

  /// Indicates whether or not state is to be retrieved from flash and how
  SwitchRestoreMode restore_mode{SWITCH_RESTORE_DEFAULT_OFF};

  /// The current reported state of the binary sensor.
  bool state;

  /** Control this switch using a boolean state value.
   *
   * This method provides a unified interface for setting the switch state based on a boolean parameter.
   * It automatically calls turn_on() when state is true or turn_off() when state is false.
   *
   * @param target_state The desired state: true to turn the switch ON, false to turn it OFF.
   */
  void control(bool target_state);
  /** Turn this switch on. This is called by the front-end.
   *
   * For implementing switches, please override write_state.
   */
  void turn_on();
  /** Turn this switch off. This is called by the front-end.
   *
   * For implementing switches, please override write_state.
   */
  void turn_off();
  /** Toggle this switch. This is called by the front-end.
   *
   * For implementing switches, please override write_state.
   */
  void toggle();

  /** Set whether the state should be treated as inverted.
   *
   * To the developer and user an inverted switch will act just like a non-inverted one.
   * In particular, the only thing that's changed by this is the value passed to
   * write_state and the state in publish_state. The .state member variable and
   * turn_on/turn_off/toggle remain unaffected.
   *
   * @param inverted Whether to invert this switch.
   */
  void set_inverted(bool inverted);

  /** Set callback for state changes.
   *
   * @param callback The void(bool) callback.
   */
  void add_on_state_callback(std::function<void(bool)> &&callback);

  /** Returns the initial state of the switch, as persisted previously,
    or empty if never persisted.
   */
  optional<bool> get_initial_state();

  /** Returns the initial state of the switch, after applying restore mode rules.
   * If restore mode is disabled, this function will return an optional with no value
   * (.has_value() is false), leaving it up to the component to decide the state.
   * For example, the component could read the state from hardware and determine the current
   * state.
   */
  optional<bool> get_initial_state_with_restore_mode();

  /** Return whether this switch uses an assumed state - i.e. if both the ON/OFF actions should be displayed in Home
   * Assistant because the real state is unknown.
   *
   * Defaults to false.
   */
  virtual bool assumed_state();

  bool is_inverted() const;

  void set_restore_mode(SwitchRestoreMode restore_mode) { this->restore_mode = restore_mode; }

 protected:
  /** Write the given state to hardware. You should implement this
   * abstract method if you want to create your own switch.
   *
   * In the implementation of this method, you should also call
   * publish_state to acknowledge that the state was written to the hardware.
   *
   * @param state The state to write. Inversion is already applied if user specified it.
   */
  virtual void write_state(bool state) = 0;

  // Pointer first (4 bytes)
  ESPPreferenceObject rtc_;

  // CallbackManager (12 bytes on 32-bit - contains vector)
  CallbackManager<void(bool)> state_callback_{};

  // Small types grouped together
  Deduplicator<bool> publish_dedup_;  // 2 bytes (bool has_value_ + bool last_value_)
  bool inverted_{false};              // 1 byte
  // Total: 3 bytes, 1 byte padding
};

#define LOG_SWITCH(prefix, type, obj) log_switch((TAG), (prefix), LOG_STR_LITERAL(type), (obj))
void log_switch(const char *tag, const char *prefix, const char *type, Switch *obj);

}  // namespace switch_
}  // namespace esphome
