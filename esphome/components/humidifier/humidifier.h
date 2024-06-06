#pragma once

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include "esphome/core/log.h"
#include "humidifier_mode.h"
#include "humidifier_traits.h"

namespace esphome {
namespace humidifier {

#define LOG_HUMIDIFIER(prefix, type, obj) \
  if ((obj) != nullptr) { \
    ESP_LOGCONFIG(TAG, "%s%s '%s'", prefix, LOG_STR_LITERAL(type), (obj)->get_name().c_str()); \
  }

class Humidifier;

/** This class is used to encode all control actions on a humidifier device.
 *
 * It is supposed to be used by all code that wishes to control a humidifier device (mqtt, api, lambda etc).
 * Create an instance of this class by calling `id(humidifier_device).make_call();`. Then set all attributes
 * with the `set_x` methods. Finally, to apply the changes call `.perform();`.
 *
 * The integration that implements the humidifier device receives this instance with the `control` method.
 * It should check all the properties it implements and apply them as needed. It should do so by
 * getting all properties it controls with the getter methods in this class. If the optional value is
 * set (check with `.has_value()`) that means the user wants to control this property. Get the value
 * of the optional with the star operator (`*call.get_mode()`) and apply it.
 */
class HumidifierCall {
 public:
  explicit HumidifierCall(Humidifier *parent) : parent_(parent) {}

  /// Set the mode of the humidifier device.
  HumidifierCall &set_mode(HumidifierMode mode);
  /// Set the mode of the humidifier device.
  HumidifierCall &set_mode(optional<HumidifierMode> mode);
  /// Set the mode of the humidifier device based on a string.
  HumidifierCall &set_mode(const std::string &mode);
  /// Set the target humidity of the humidifier device.
  HumidifierCall &set_target_humidity(float target_humidity);
  /// Set the target humidity of the humidifier device.
  HumidifierCall &set_target_humidity(optional<float> target_humidity);
  /** Set the low point target humidity of the humidifier device
   *
   * For humidifier devices with two point target humidity control
   */
  HumidifierCall &set_target_humidity_low(float target_humidity_low);
  /** Set the low point target humidity of the humidifier device
   *
   * For humidifier devices with two point target humidity control
   */
  HumidifierCall &set_target_humidity_low(optional<float> target_humidity_low);
  /** Set the high point target humidity of the humidifier device
   *
   * For humidifier devices with two point target humidity control
   */
  HumidifierCall &set_target_humidity_high(float target_humidity_high);
  /** Set the high point target humidity of the humidifier device
   *
   * For humidifier devices with two point target humidity control
   */
  HumidifierCall &set_target_humidity_high(optional<float> target_humidity_high);
  /// Set the preset of the humidifier device.
  HumidifierCall &set_preset(HumidifierPreset preset);
  /// Set the preset of the humidifier device.
  HumidifierCall &set_preset(optional<HumidifierPreset> preset);
  /// Set the preset of the humidifier device based on a string.
  HumidifierCall &set_preset(const std::string &preset);
  /// Set the preset of the humidifier device based on a string.
  HumidifierCall &set_preset(optional<std::string> preset);

  void perform();

  const optional<HumidifierMode> &get_mode() const;
  const optional<float> &get_target_humidity() const;
  const optional<float> &get_target_humidity_low() const;
  const optional<float> &get_target_humidity_high() const;
  const optional<HumidifierPreset> &get_preset() const;

 protected:
  void validate_();

  Humidifier *const parent_;
  optional<HumidifierMode> mode_;
  optional<float> target_humidity_;
  optional<float> target_humidity_low_;
  optional<float> target_humidity_high_;
  optional<HumidifierPreset> preset_;
};

/// Struct used to save the state of the humidifier device in restore memory.
/// Make sure to update RESTORE_STATE_VERSION when changing the struct entries.
struct HumidifierDeviceRestoreState {
  HumidifierMode mode;
  HumidifierPreset preset;
  union {
    float target_humidity;
    struct {
      float target_humidity_low;
      float target_humidity_high;
    };
  };

  /// Convert this struct to a humidifier call that can be performed.
  HumidifierCall to_call(Humidifier *humidifier);
  /// Apply these settings to the humidifier device.
  void apply(Humidifier *humidifier);
} __attribute__((packed));

/**
 * HumidifierDevice - This is the base class for all humidifier integrations. Each integration
 * needs to extend this class and implement two functions:
 *
 *  - get_traits() - return the static traits of the humidifier device
 *  - control(HumidifierDeviceCall call) - Apply the given changes from call.
 *
 * To write data to the frontend, the integration must first set the properties using
 * this->property = value; (for example this->current_humidity = 42.0;); then the integration
 * must call this->publish_state(); to send the entire state to the frontend.
 *
 * The entire state of the humidifier device is encoded in public properties of the base class (current_humidity,
 * mode etc). These are read-only for the user and rw for integrations. The reason these are public
 * is for simple access to them from lambdas `if (id(my_humidifier).mode ==
 * humidifier::HUMIDIFIER_MODE_HUMIDIFY_DEHUMIDIFY) ...`
 */
class Humidifier : public EntityBase {
 public:
  /// Construct a humidifier device with empty name (will be set later).
  Humidifier();
  // /// Construct a humidifier device with a name.
  // Humidifier(const std::string &name);

  /// The active mode of the humidifier device.
  HumidifierMode mode{HUMIDIFIER_MODE_OFF};
  /// The active state of the humidifier device.
  HumidifierAction action{HUMIDIFIER_ACTION_OFF};
  /// The current humidity of the humidifier device, as reported from the integration.
  float current_humidity{NAN};

  union {
    /// The target humidity of the humidifier device.
    float target_humidity;
    struct {
      /// The minimum target humidity of the humidifier device, for humidifier devices with split target humidity.
      float target_humidity_low;
      /// The maximum target humidity of the humidifier device, for humidifier devices with split target humidity.
      float target_humidity_high;
    };
  };

  /// The active preset of the humidifier device.
  optional<HumidifierPreset> preset;

  /** Add a callback for the humidifier device state, each time the state of the humidifier device is updated
   * (using publish_state), this callback will be called.
   *
   * @param callback The callback to call.
   */
  void add_on_state_callback(std::function<void()> &&callback);

  /** Make a humidifier device control call, this is used to control the humidifier device, see the HumidifierCall
   * description for more info.
   * @return A new HumidifierCall instance targeting this humidifier device.
   */
  HumidifierCall make_call();

  /** Publish the state of the humidifier device, to be called from integrations.
   *
   * This will schedule the humidifier device to publish its state to all listeners and save the current state
   * to recover memory.
   */
  void publish_state();

  /** Get the traits of this humidifier device with all overrides applied.
   *
   * Traits are static data that encode the capabilities and static data for a humidifier device such as supported
   * modes, humidity range etc.
   */
  HumidifierTraits get_traits();

  void set_visual_min_humidity_override(float visual_min_humidity_override);
  void set_visual_max_humidity_override(float visual_max_humidity_override);
  void set_visual_humidity_step_override(float visual_humidity_step_override);

 protected:
  friend HumidifierCall;

  /// Set preset. Reset custom preset. Return true if preset has been changed.
  bool set_preset_(HumidifierPreset preset);

  /** Get the default traits of this humidifier device.
   *
   * Traits are static data that encode the capabilities and static data for a humidifier device such as supported
   * modes, humidity range etc. Each integration must implement this method and the return value must
   * be constant during all of execution time.
   */
  virtual HumidifierTraits traits() = 0;

  /** Control the humidifier device, this is a virtual method that each humidifier integration must implement.
   *
   * See more info in HumidifierCall. The integration should check all of its values in this method and
   * set them accordingly. At the end of the call, the integration must call `publish_state()` to
   * notify the frontend of a changed state.
   *
   * @param call The HumidifierCall instance encoding all attribute changes.
   */
  virtual void control(const HumidifierCall &call) = 0;

  /// Restore the state of the humidifier device, call this from your setup() method.
  optional<HumidifierDeviceRestoreState> restore_state_();

  /** Internal method to save the state of the humidifier device to recover memory. This is automatically
   * called from publish_state()
   */
  void save_state_();

  uint32_t hash_base() override;
  void dump_traits_(const char *tag);

  CallbackManager<void()> state_callback_{};
  ESPPreferenceObject rtc_;
  optional<float> visual_min_humidity_override_{};
  optional<float> visual_max_humidity_override_{};
  optional<float> visual_humidity_step_override_{};
};

}  // namespace humidifier
}  // namespace esphome
