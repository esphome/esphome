#pragma once

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace switch_ {

#define LOG_SWITCH(prefix, type, obj) \
  if (obj != nullptr) { \
    ESP_LOGCONFIG(TAG, "%s%s '%s'", prefix, type, obj->get_name().c_str()); \
    if (!obj->get_icon().empty()) { \
      ESP_LOGCONFIG(TAG, "%s  Icon: '%s'", prefix, obj->get_icon().c_str()); \
    } \
    if (obj->assumed_state()) { \
      ESP_LOGCONFIG(TAG, "%s  Assumed State: YES", prefix); \
    } \
    if (obj->is_inverted()) { \
      ESP_LOGCONFIG(TAG, "%s  Inverted: YES", prefix); \
    } \
  }

/** Base class for all switches.
 *
 * A switch is basically just a combination of a binary sensor (for reporting switch values)
 * and a write_state method that writes a state to the hardware.
 */
class Switch : public Nameable {
 public:
  explicit Switch();
  explicit Switch(const std::string &name);

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

  /// Set the icon for this switch. "" for no icon.
  void set_icon(const std::string &icon);

  /// Get the icon for this switch. Using icon() if not manually set
  std::string get_icon();

  /** Set callback for state changes.
   *
   * @param callback The void(bool) callback.
   */
  void add_on_state_callback(std::function<void(bool)> &&callback);

  optional<bool> get_initial_state();

  /** Return whether this switch uses an assumed state - i.e. if both the ON/OFF actions should be displayed in Home
   * Assistant because the real state is unknown.
   *
   * Defaults to false.
   */
  virtual bool assumed_state();

  bool is_inverted() const;

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

  /** Override this to set the Home Assistant icon for this switch.
   *
   * Return "" to disable this feature.
   *
   * @return The icon of this switch, for example "mdi:fan".
   */
  virtual std::string icon();  // NOLINT

  uint32_t hash_base() override;

  optional<std::string> icon_{};  ///< The icon shown here. Not set means use default from switch. Empty means no icon.

  CallbackManager<void(bool)> state_callback_{};
  bool inverted_{false};
  Deduplicator<bool> publish_dedup_;
  ESPPreferenceObject rtc_;
};

}  // namespace switch_
}  // namespace esphome
