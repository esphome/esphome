#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include "climate_mode.h"
#include "climate_traits.h"

namespace esphome {
namespace climate {

#define LOG_CLIMATE(prefix, type, obj) \
  if (obj != nullptr) { \
    ESP_LOGCONFIG(TAG, "%s%s '%s'", prefix, type, obj->get_name().c_str()); \
  }

class Climate;

/** This class is used to encode all control actions on a climate device.
 *
 * It is supposed to be used by all code that wishes to control a climate device (mqtt, api, lambda etc).
 * Create an instance of this class by calling `id(climate_device).make_call();`. Then set all attributes
 * with the `set_x` methods. Finally, to apply the changes call `.perform();`.
 *
 * The integration that implements the climate device receives this instance with the `control` method.
 * It should check all the properties it implements and apply them as needed. It should do so by
 * getting all properties it controls with the getter methods in this class. If the optional value is
 * set (check with `.has_value()`) that means the user wants to control this property. Get the value
 * of the optional with the star operator (`*call.get_mode()`) and apply it.
 */
class ClimateCall {
 public:
  explicit ClimateCall(Climate *parent) : parent_(parent) {}

  /// Set the mode of the climate device.
  ClimateCall &set_mode(ClimateMode mode);
  /// Set the mode of the climate device.
  ClimateCall &set_mode(optional<ClimateMode> mode);
  /// Set the mode of the climate device based on a string.
  ClimateCall &set_mode(const std::string &mode);
  /// Set the target temperature of the climate device.
  ClimateCall &set_target_temperature(float target_temperature);
  /// Set the target temperature of the climate device.
  ClimateCall &set_target_temperature(optional<float> target_temperature);
  /** Set the low point target temperature of the climate device
   *
   * For climate devices with two point target temperature control
   */
  ClimateCall &set_target_temperature_low(float target_temperature_low);
  /** Set the low point target temperature of the climate device
   *
   * For climate devices with two point target temperature control
   */
  ClimateCall &set_target_temperature_low(optional<float> target_temperature_low);
  /** Set the high point target temperature of the climate device
   *
   * For climate devices with two point target temperature control
   */
  ClimateCall &set_target_temperature_high(float target_temperature_high);
  /** Set the high point target temperature of the climate device
   *
   * For climate devices with two point target temperature control
   */
  ClimateCall &set_target_temperature_high(optional<float> target_temperature_high);
  ClimateCall &set_away(bool away);
  ClimateCall &set_away(optional<bool> away);

  void perform();

  const optional<ClimateMode> &get_mode() const;
  const optional<float> &get_target_temperature() const;
  const optional<float> &get_target_temperature_low() const;
  const optional<float> &get_target_temperature_high() const;
  const optional<bool> &get_away() const;

 protected:
  void validate_();

  Climate *const parent_;
  optional<ClimateMode> mode_;
  optional<float> target_temperature_;
  optional<float> target_temperature_low_;
  optional<float> target_temperature_high_;
  optional<bool> away_;
};

/// Struct used to save the state of the climate device in restore memory.
struct ClimateDeviceRestoreState {
  ClimateMode mode;
  bool away;
  union {
    float target_temperature;
    struct {
      float target_temperature_low;
      float target_temperature_high;
    };
  };

  /// Convert this struct to a climate call that can be performed.
  ClimateCall to_call(Climate *climate);
  /// Apply these settings to the climate device.
  void apply(Climate *climate);
} __attribute__((packed));

/**
 * ClimateDevice - This is the base class for all climate integrations. Each integration
 * needs to extend this class and implement two functions:
 *
 *  - get_traits() - return the static traits of the climate device
 *  - control(ClimateDeviceCall call) - Apply the given changes from call.
 *
 * To write data to the frontend, the integration must first set the properties using
 * this->property = value; (for example this->current_temperature = 42.0;); then the integration
 * must call this->publish_state(); to send the entire state to the frontend.
 *
 * The entire state of the climate device is encoded in public properties of the base class (current_temperature,
 * mode etc). These are read-only for the user and rw for integrations. The reason these are public
 * is for simple access to them from lambdas `if (id(my_climate).mode == climate::CLIMATE_MODE_AUTO) ...`
 */
class Climate : public Nameable {
 public:
  /// Construct a climate device with empty name (will be set later).
  Climate();
  /// Construct a climate device with a name.
  Climate(const std::string &name);

  /// The active mode of the climate device.
  ClimateMode mode{CLIMATE_MODE_OFF};
  /// The active state of the climate device.
  ClimateAction action{CLIMATE_ACTION_OFF};
  /// The current temperature of the climate device, as reported from the integration.
  float current_temperature{NAN};

  union {
    /// The target temperature of the climate device.
    float target_temperature;
    struct {
      /// The minimum target temperature of the climate device, for climate devices with split target temperature.
      float target_temperature_low;
      /// The maximum target temperature of the climate device, for climate devices with split target temperature.
      float target_temperature_high;
    };
  };

  /** Whether the climate device is in away mode.
   *
   * Away allows climate devices to have two different target temperature configs:
   * one for normal mode and one for away mode.
   */
  bool away{false};

  /** Add a callback for the climate device state, each time the state of the climate device is updated
   * (using publish_state), this callback will be called.
   *
   * @param callback The callback to call.
   */
  void add_on_state_callback(std::function<void()> &&callback);

  /** Make a climate device control call, this is used to control the climate device, see the ClimateCall description
   * for more info.
   * @return A new ClimateCall instance targeting this climate device.
   */
  ClimateCall make_call();

  /** Publish the state of the climate device, to be called from integrations.
   *
   * This will schedule the climate device to publish its state to all listeners and save the current state
   * to recover memory.
   */
  void publish_state();

  /** Get the traits of this climate device with all overrides applied.
   *
   * Traits are static data that encode the capabilities and static data for a climate device such as supported
   * modes, temperature range etc.
   */
  ClimateTraits get_traits();

  void set_visual_min_temperature_override(float visual_min_temperature_override);
  void set_visual_max_temperature_override(float visual_max_temperature_override);
  void set_visual_temperature_step_override(float visual_temperature_step_override);

 protected:
  friend ClimateCall;

  /** Get the default traits of this climate device.
   *
   * Traits are static data that encode the capabilities and static data for a climate device such as supported
   * modes, temperature range etc. Each integration must implement this method and the return value must
   * be constant during all of execution time.
   */
  virtual ClimateTraits traits() = 0;

  /** Control the climate device, this is a virtual method that each climate integration must implement.
   *
   * See more info in ClimateCall. The integration should check all of its values in this method and
   * set them accordingly. At the end of the call, the integration must call `publish_state()` to
   * notify the frontend of a changed state.
   *
   * @param call The ClimateCall instance encoding all attribute changes.
   */
  virtual void control(const ClimateCall &call) = 0;
  /// Restore the state of the climate device, call this from your setup() method.
  optional<ClimateDeviceRestoreState> restore_state_();
  /** Internal method to save the state of the climate device to recover memory. This is automatically
   * called from publish_state()
   */
  void save_state_();

  uint32_t hash_base() override;

  CallbackManager<void()> state_callback_{};
  ESPPreferenceObject rtc_;
  optional<float> visual_min_temperature_override_{};
  optional<float> visual_max_temperature_override_{};
  optional<float> visual_temperature_step_override_{};
};

}  // namespace climate
}  // namespace esphome
