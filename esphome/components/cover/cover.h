#pragma once

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include "cover_traits.h"

namespace esphome {
namespace cover {

const extern float COVER_OPEN;
const extern float COVER_CLOSED;

#define LOG_COVER(prefix, type, obj) \
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

class Cover;

class CoverCall {
 public:
  CoverCall(Cover *parent);

  /// Set the command as a string, "STOP", "OPEN", "CLOSE", "TOGGLE".
  CoverCall &set_command(const char *command);
  /// Set the command to open the cover.
  CoverCall &set_command_open();
  /// Set the command to close the cover.
  CoverCall &set_command_close();
  /// Set the command to stop the cover.
  CoverCall &set_command_stop();
  /// Set the command to toggle the cover.
  CoverCall &set_command_toggle();
  /// Set the call to a certain target position.
  CoverCall &set_position(float position);
  /// Set the call to a certain target tilt.
  CoverCall &set_tilt(float tilt);
  /// Set whether this cover call should stop the cover.
  CoverCall &set_stop(bool stop);

  /// Perform the cover call.
  void perform();

  const optional<float> &get_position() const;
  bool get_stop() const;
  const optional<float> &get_tilt() const;
  const optional<bool> &get_toggle() const;

 protected:
  void validate_();

  Cover *parent_;
  bool stop_{false};
  optional<float> position_{};
  optional<float> tilt_{};
  optional<bool> toggle_{};
};

/// Struct used to store the restored state of a cover
struct CoverRestoreState {
  float position;
  float tilt;

  /// Convert this struct to a cover call that can be performed.
  CoverCall to_call(Cover *cover);
  /// Apply these settings to the cover
  void apply(Cover *cover);
} __attribute__((packed));

/// Enum encoding the current operation of a cover.
enum CoverOperation : uint8_t {
  /// The cover is currently idle (not moving)
  COVER_OPERATION_IDLE = 0,
  /// The cover is currently opening.
  COVER_OPERATION_OPENING,
  /// The cover is currently closing.
  COVER_OPERATION_CLOSING,
};

const char *cover_operation_to_str(CoverOperation op);

/** Base class for all cover devices.
 *
 * Covers currently have three properties:
 *  - position - The current position of the cover from 0.0 (fully closed) to 1.0 (fully open).
 *    For covers with only binary OPEN/CLOSED position this will always be either 0.0 or 1.0
 *  - tilt - The tilt value of the cover from 0.0 (closed) to 1.0 (closed)
 *  - current_operation - The operation the cover is currently performing, this can
 *    be one of IDLE, OPENING and CLOSING.
 *
 * For users: All cover operations must be performed over the .make_call() interface.
 * To command a cover, use .make_call() to create a call object, set all properties
 * you wish to set, and activate the command with .perform().
 * For reading out the current values of the cover, use the public .position, .tilt etc
 * properties (these are read-only for users)
 *
 * For integrations: Integrations must implement two methods: control() and get_traits().
 * Control will be called with the arguments supplied by the user and should be used
 * to control all values of the cover. Also implement get_traits() to return what operations
 * the cover supports.
 */
class Cover : public EntityBase {
 public:
  explicit Cover();
  explicit Cover(const std::string &name);

  /// The current operation of the cover (idle, opening, closing).
  CoverOperation current_operation{COVER_OPERATION_IDLE};
  /** The position of the cover from 0.0 (fully closed) to 1.0 (fully open).
   *
   * For binary covers this is always equals to 0.0 or 1.0 (see also COVER_OPEN and
   * COVER_CLOSED constants).
   */
  float position;
  /// The current tilt value of the cover from 0.0 to 1.0.
  float tilt{COVER_OPEN};

  /// Construct a new cover call used to control the cover.
  CoverCall make_call();
  /** Open the cover.
   *
   * This is a legacy method and may be removed later, please use `.make_call()` instead.
   */
  ESPDEPRECATED("open() is deprecated, use make_call().set_command_open() instead.", "2021.9")
  void open();
  /** Close the cover.
   *
   * This is a legacy method and may be removed later, please use `.make_call()` instead.
   */
  ESPDEPRECATED("close() is deprecated, use make_call().set_command_close() instead.", "2021.9")
  void close();
  /** Stop the cover.
   *
   * This is a legacy method and may be removed later, please use `.make_call()` instead.
   */
  ESPDEPRECATED("stop() is deprecated, use make_call().set_command_stop() instead.", "2021.9")
  void stop();

  void add_on_state_callback(std::function<void()> &&f);

  /** Publish the current state of the cover.
   *
   * First set the .position, .tilt, etc values and then call this method
   * to publish the state of the cover.
   *
   * @param save Whether to save the updated values in RTC area.
   */
  void publish_state(bool save = true);

  virtual CoverTraits get_traits() = 0;
  void set_device_class(const std::string &device_class);
  std::string get_device_class();

  /// Helper method to check if the cover is fully open. Equivalent to comparing .position against 1.0
  bool is_fully_open() const;
  /// Helper method to check if the cover is fully closed. Equivalent to comparing .position against 0.0
  bool is_fully_closed() const;

 protected:
  friend CoverCall;

  virtual void control(const CoverCall &call) = 0;

  /** Override this to set the default device class.
   *
   * @deprecated This method is deprecated, set the property during config validation instead. (2022.1)
   */
  virtual std::string device_class();

  optional<CoverRestoreState> restore_state_();

  CallbackManager<void()> state_callback_{};
  optional<std::string> device_class_override_{};

  ESPPreferenceObject rtc_;
};

}  // namespace cover
}  // namespace esphome
