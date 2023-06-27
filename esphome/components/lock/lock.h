#pragma once

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/preferences.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <set>

namespace esphome {
namespace lock {

class Lock;

#define LOG_LOCK(prefix, type, obj) \
  if ((obj) != nullptr) { \
    ESP_LOGCONFIG(TAG, "%s%s '%s'", prefix, LOG_STR_LITERAL(type), (obj)->get_name().c_str()); \
    if (!(obj)->get_icon().empty()) { \
      ESP_LOGCONFIG(TAG, "%s  Icon: '%s'", prefix, (obj)->get_icon().c_str()); \
    } \
    if ((obj)->traits.get_assumed_state()) { \
      ESP_LOGCONFIG(TAG, "%s  Assumed State: YES", prefix); \
    } \
  }
/// Enum for all states a lock can be in.
enum LockState : uint8_t {
  LOCK_STATE_NONE = 0,
  LOCK_STATE_LOCKED = 1,
  LOCK_STATE_UNLOCKED = 2,
  LOCK_STATE_JAMMED = 3,
  LOCK_STATE_LOCKING = 4,
  LOCK_STATE_UNLOCKING = 5
};
const char *lock_state_to_string(LockState state);

class LockTraits {
 public:
  LockTraits() = default;

  bool get_supports_open() const { return this->supports_open_; }
  void set_supports_open(bool supports_open) { this->supports_open_ = supports_open; }
  bool get_requires_code() const { return this->requires_code_; }
  void set_requires_code(bool requires_code) { this->requires_code_ = requires_code; }
  bool get_assumed_state() const { return this->assumed_state_; }
  void set_assumed_state(bool assumed_state) { this->assumed_state_ = assumed_state; }

  bool supports_state(LockState state) const { return supported_states_.count(state); }
  std::set<LockState> get_supported_states() const { return supported_states_; }
  void set_supported_states(std::set<LockState> states) { supported_states_ = std::move(states); }
  void add_supported_state(LockState state) { supported_states_.insert(state); }

 protected:
  bool supports_open_{false};
  bool requires_code_{false};
  bool assumed_state_{false};
  std::set<LockState> supported_states_ = {LOCK_STATE_NONE, LOCK_STATE_LOCKED, LOCK_STATE_UNLOCKED};
};

/** This class is used to encode all control actions on a lock device.
 *
 * It is supposed to be used by all code that wishes to control a lock device (mqtt, api, lambda etc).
 * Create an instance of this class by calling `id(lock_device).make_call();`. Then set all attributes
 * with the `set_x` methods. Finally, to apply the changes call `.perform();`.
 *
 * The integration that implements the lock device receives this instance with the `control` method.
 * It should check all the properties it implements and apply them as needed. It should do so by
 * getting all properties it controls with the getter methods in this class. If the optional value is
 * set (check with `.has_value()`) that means the user wants to control this property. Get the value
 * of the optional with the star operator (`*call.get_state()`) and apply it.
 */
class LockCall {
 public:
  LockCall(Lock *parent) : parent_(parent) {}

  /// Set the state of the lock device.
  LockCall &set_state(LockState state);
  /// Set the state of the lock device.
  LockCall &set_state(optional<LockState> state);
  /// Set the state of the lock device based on a string.
  LockCall &set_state(const std::string &state);

  void perform();

  const optional<LockState> &get_state() const;

 protected:
  void validate_();

  Lock *const parent_;
  optional<LockState> state_;
};

/** Base class for all locks.
 *
 * A lock is basically a switch with a combination of a binary sensor (for reporting lock values)
 * and a write_state method that writes a state to the hardware. Locks can also have an "open"
 * method to unlatch.
 *
 * For integrations: Integrations must implement the method control().
 * Control will be called with the arguments supplied by the user and should be used
 * to control all values of the lock.
 */
class Lock : public EntityBase {
 public:
  explicit Lock();

  /** Make a lock device control call, this is used to control the lock device, see the LockCall description
   * for more info.
   * @return A new LockCall instance targeting this lock device.
   */
  LockCall make_call();

  /** Publish a state to the front-end from the back-end.
   *
   * Then the internal value member is set and finally the callbacks are called.
   *
   * @param state The new state.
   */
  void publish_state(LockState state);

  /// The current reported state of the lock.
  LockState state{LOCK_STATE_NONE};

  LockTraits traits;

  /** Turn this lock on. This is called by the front-end.
   *
   * For implementing locks, please override control.
   */
  void lock();
  /** Turn this lock off. This is called by the front-end.
   *
   * For implementing locks, please override control.
   */
  void unlock();
  /** Open (unlatch) this lock. This is called by the front-end.
   *
   * For implementing locks, please override control.
   */
  void open();

  /** Set callback for state changes.
   *
   * @param callback The void(bool) callback.
   */
  void add_on_state_callback(std::function<void()> &&callback);

 protected:
  friend LockCall;

  /** Perform the open latch action with hardware. This method is optional to implement
   * when creating a new lock.
   *
   * In the implementation of this method, it is recommended you also call
   * publish_state with "unlock" to acknowledge that the state was written to the hardware.
   */
  virtual void open_latch() { unlock(); };

  /** Control the lock device, this is a virtual method that each lock integration must implement.
   *
   * See more info in LockCall. The integration should check all of its values in this method and
   * set them accordingly. At the end of the call, the integration must call `publish_state()` to
   * notify the frontend of a changed state.
   *
   * @param call The LockCall instance encoding all attribute changes.
   */
  virtual void control(const LockCall &call) = 0;

  CallbackManager<void()> state_callback_{};
  Deduplicator<LockState> publish_dedup_;
  ESPPreferenceObject rtc_;
};

}  // namespace lock
}  // namespace esphome
