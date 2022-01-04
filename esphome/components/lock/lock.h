#pragma once

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/preferences.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace lock {

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
const char *lock_state_to_string(LockState mode);

class LockTraits {
 public:
  LockTraits() = default;

  bool get_supports_open() const { return this->supports_open_; }
  void set_supports_open(bool supports_open) { this->supports_open_ = supports_open; }
  bool get_requires_code() const { return this->requires_code_; }
  void set_requires_code(bool requires_code) { this->requires_code_ = requires_code; }
  bool get_assumed_state() const { return this->assumed_state_; }
  void set_assumed_state(bool assumed_state) { this->assumed_state_ = assumed_state; }

 protected:
  bool supports_open_{false};
  bool requires_code_{false};
  bool assumed_state_{false};
};

/** Base class for all locks.
 *
 * A lock is basically a switch with a combination of a binary sensor (for reporting lock values)
 * and a write_state method that writes a state to the hardware. Locks can also have an "open"
 * method to unlatch.
 */
class Lock : public EntityBase {
 public:
  explicit Lock();
  explicit Lock(const std::string &name);

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
   * For implementing locks, please override write_state.
   */
  void lock();
  /** Turn this lock off. This is called by the front-end.
   *
   * For implementing locks, please override write_state.
   */
  void unlock();
  /** Open (unlatch) this lock. This is called by the front-end.
   *
   * For implementing locks, please override write_state.
   */
  void open();

  /** Set callback for state changes.
   *
   * @param callback The void(bool) callback.
   */
  void add_on_state_callback(std::function<void()> &&callback);

  /** Return whether this lock uses an assumed state - i.e. if both the LOCK/UNLOCK actions should be displayed in Home
   * Assistant because the real state is unknown.
   *
   * Defaults to false.
   */

 protected:
  /** Write the given state to hardware. You should implement this
   * abstract method if you want to create your own lock.
   *
   * In the implementation of this method, you should also call
   * publish_state to acknowledge that the state was written to the hardware.
   *
   * @param state The state to write.
   */
  virtual void write_state(LockState state) = 0;

  /** Perform the open latch action with hardware. This method is optional to implement
   * when creating a new lock.
   *
   * In the implementation of this method, it is recommended you also call
   * publish_state with "unlock" to acknowledge that the state was written to the hardware.
   */
  virtual void open_latch() { unlock(); };

  uint32_t hash_base() override;

  CallbackManager<void()> state_callback_{};
  Deduplicator<LockState> publish_dedup_;
  ESPPreferenceObject rtc_;
};

}  // namespace lock
}  // namespace esphome
