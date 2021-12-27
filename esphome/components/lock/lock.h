#pragma once

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/preferences.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace lock_ {

#define LOCKUNLOCK(b) ((b) ? "LOCKED" : "UNLOCKED")

#define LOG_LOCK(prefix, type, obj) \
  if ((obj) != nullptr) { \
    ESP_LOGCONFIG(TAG, "%s%s '%s'", prefix, LOG_STR_LITERAL(type), (obj)->get_name().c_str()); \
    if (!(obj)->get_icon().empty()) { \
      ESP_LOGCONFIG(TAG, "%s  Icon: '%s'", prefix, (obj)->get_icon().c_str()); \
    } \
    if ((obj)->assumed_state()) { \
      ESP_LOGCONFIG(TAG, "%s  Assumed State: YES", prefix); \
    } \
    if ((obj)->is_inverted()) { \
      ESP_LOGCONFIG(TAG, "%s  Inverted: YES", prefix); \
    } \
  }

/** Base class for all locks.
 *
 * A lock is basically a switch with a combination of a binary sensor (for reporting lock values)
 * and a write_state method that writes a state to the hardware. Locks can also have an "open"
 * method to unlatch
 */
class Lock : public EntityBase {
 public:
  explicit Lock();
  explicit Lock(const std::string &name);

  /** Publish a state to the front-end from the back-end.
   *
   * The input value is inverted if applicable. Then the internal value member is set and
   * finally the callbacks are called.
   *
   * @param state The new state.
   */
  void publish_state(bool state);

  /// The current reported state of the binary sensor.
  bool state;

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
  /** Toggle this lock. This is called by the front-end.
   *
   * For implementing locks, please override write_state.
   */
  void toggle();
  /** Open (unlatch) this lock. This is called by the front-end.
   *
   * For implementing locks, please override write_state.
   */
  void open();

  /** Set whether the state should be treated as inverted.
   *
   * To the developer and user an inverted lock will act just like a non-inverted one.
   * In particular, the only thing that's changed by this is the value passed to
   * write_state and the state in publish_state. The .state member variable and
   * turn_on/turn_off/toggle remain unaffected.
   *
   * @param inverted Whether to invert this lock.
   */
  void set_inverted(bool inverted);

  /** Set callback for state changes.
   *
   * @param callback The void(bool) callback.
   */
  void add_on_state_callback(std::function<void(bool)> &&callback);

  optional<bool> get_initial_state();

  /** Return whether this lock uses an assumed state - i.e. if both the ON/OFF actions should be displayed in Home
   * Assistant because the real state is unknown.
   *
   * Defaults to false.
   */
  virtual bool assumed_state();

  bool is_inverted() const;

 protected:
  /** Write the given state to hardware. You should implement this
   * abstract method if you want to create your own lock.
   *
   * In the implementation of this method, you should also call
   * publish_state to acknowledge that the state was written to the hardware.
   *
   * @param state The state to write. Inversion is already applied if user specified it.
   */
  virtual void write_state(bool state) = 0;
  
  /** Perform the open latch action with hardware. This method is optional to implement
   * when creating a new lock.
   *
   * In the implementation of this method, it is recommended you also call
   * publish_state with "unlock" to acknowledge that the state was written to the hardware.
   *
   * @param state The state to write. Inversion is already applied if user specified it.
   */
  virtual void open_latch() = 0;

  uint32_t hash_base() override;

  CallbackManager<void(bool)> state_callback_{};
  bool inverted_{false};
  Deduplicator<bool> publish_dedup_;
  ESPPreferenceObject rtc_;
};

}  // namespace lock_
}  // namespace esphome
