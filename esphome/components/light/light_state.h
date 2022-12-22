#pragma once

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/optional.h"
#include "esphome/core/preferences.h"
#include "light_call.h"
#include "light_color_values.h"
#include "light_effect.h"
#include "light_traits.h"
#include "light_transformer.h"

#include <vector>

namespace esphome {
namespace light {

class LightOutput;

enum LightRestoreMode {
  LIGHT_RESTORE_DEFAULT_OFF,
  LIGHT_RESTORE_DEFAULT_ON,
  LIGHT_ALWAYS_OFF,
  LIGHT_ALWAYS_ON,
  LIGHT_RESTORE_INVERTED_DEFAULT_OFF,
  LIGHT_RESTORE_INVERTED_DEFAULT_ON,
  LIGHT_RESTORE_AND_OFF,
  LIGHT_RESTORE_AND_ON,
};

/** This class represents the communication layer between the front-end MQTT layer and the
 * hardware output layer.
 */
class LightState : public EntityBase, public Component {
 public:
  /// Construct this LightState using the provided traits and name.
  LightState(const std::string &name, LightOutput *output);

  LightState(LightOutput *output);

  LightTraits get_traits();

  /// Make a light state call
  LightCall turn_on();
  LightCall turn_off();
  LightCall toggle();
  LightCall make_call();

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  /// Load state from preferences
  void setup() override;
  void dump_config() override;
  void loop() override;
  /// Shortly after HARDWARE.
  float get_setup_priority() const override;

  /** The current values of the light as outputted to the light.
   *
   * These values represent the "real" state of the light - During transitions this
   * property will be changed continuously (in contrast to .remote_values, where they
   * are constant during transitions).
   *
   * This value does not have gamma correction applied.
   *
   * This property is read-only for users. Any changes to it will be ignored.
   */
  LightColorValues current_values;

  /** The remote color values reported to the frontend.
   *
   * These are different from the "current" values: For example transitions will
   * continuously change the "current" values. But the remote values will immediately
   * switch to the target value for a transition, reducing the number of packets sent.
   *
   * This value does not have gamma correction applied.
   *
   * This property is read-only for users. Any changes to it will be ignored.
   */
  LightColorValues remote_values;

  /// Publish the currently active state to the frontend.
  void publish_state();

  /// Get the light output associated with this object.
  LightOutput *get_output() const;

  /// Return the name of the current effect, or if no effect is active "None".
  std::string get_effect_name();

  /**
   * This lets front-end components subscribe to light change events. This callback is called once
   * when the remote color values are changed.
   *
   * @param send_callback The callback.
   */
  void add_new_remote_values_callback(std::function<void()> &&send_callback);

  /**
   * The callback is called once the state of current_values and remote_values are equal (when the
   * transition is finished).
   *
   * @param send_callback
   */
  void add_new_target_state_reached_callback(std::function<void()> &&send_callback);

  /// Set the default transition length, i.e. the transition length when no transition is provided.
  void set_default_transition_length(uint32_t default_transition_length);
  uint32_t get_default_transition_length() const;

  /// Set the flash transition length
  void set_flash_transition_length(uint32_t flash_transition_length);
  uint32_t get_flash_transition_length() const;

  /// Set the gamma correction factor
  void set_gamma_correct(float gamma_correct);
  float get_gamma_correct() const { return this->gamma_correct_; }

  /// Set the restore mode of this light
  void set_restore_mode(LightRestoreMode restore_mode);

  /// Return whether the light has any effects that meet the trait requirements.
  bool supports_effects();

  /// Get all effects for this light state.
  const std::vector<LightEffect *> &get_effects() const;

  /// Add effects for this light state.
  void add_effects(const std::vector<LightEffect *> &effects);

  /// The result of all the current_values_as_* methods have gamma correction applied.
  void current_values_as_binary(bool *binary);

  void current_values_as_brightness(float *brightness);

  void current_values_as_rgb(float *red, float *green, float *blue, bool color_interlock = false);

  void current_values_as_rgbw(float *red, float *green, float *blue, float *white, bool color_interlock = false);

  void current_values_as_rgbww(float *red, float *green, float *blue, float *cold_white, float *warm_white,
                               bool constant_brightness = false);

  void current_values_as_rgbct(float *red, float *green, float *blue, float *color_temperature,
                               float *white_brightness);

  void current_values_as_cwww(float *cold_white, float *warm_white, bool constant_brightness = false);

  void current_values_as_ct(float *color_temperature, float *white_brightness);

 protected:
  friend LightOutput;
  friend LightCall;
  friend class AddressableLight;

  /// Internal method to start an effect with the given index
  void start_effect_(uint32_t effect_index);
  /// Internal method to get the currently active effect
  LightEffect *get_active_effect_();
  /// Internal method to stop the current effect (if one is active).
  void stop_effect_();
  /// Internal method to start a transition to the target color with the given length.
  void start_transition_(const LightColorValues &target, uint32_t length, bool set_remote_values);

  /// Internal method to start a flash for the specified amount of time.
  void start_flash_(const LightColorValues &target, uint32_t length, bool set_remote_values);

  /// Internal method to set the color values to target immediately (with no transition).
  void set_immediately_(const LightColorValues &target, bool set_remote_values);

  /// Internal method to save the current remote_values to the preferences
  void save_remote_values_();

  /// Store the output to allow effects to have more access.
  LightOutput *output_;
  /// Value for storing the index of the currently active effect. 0 if no effect is active
  uint32_t active_effect_index_{};
  /// The currently active transformer for this light (transition/flash).
  std::unique_ptr<LightTransformer> transformer_{nullptr};
  /// Whether the light value should be written in the next cycle.
  bool next_write_{true};

  /// Object used to store the persisted values of the light.
  ESPPreferenceObject rtc_;

  /** Callback to call when new values for the frontend are available.
   *
   * "Remote values" are light color values that are reported to the frontend and have a lower
   * publish frequency than the "real" color values. For example, during transitions the current
   * color value may change continuously, but the remote values will be reported as the target values
   * starting with the beginning of the transition.
   */
  CallbackManager<void()> remote_values_callback_{};

  /** Callback to call when the state of current_values and remote_values are equal
   * This should be called once the state of current_values changed and equals the state of remote_values
   */
  CallbackManager<void()> target_state_reached_callback_{};

  /// Default transition length for all transitions in ms.
  uint32_t default_transition_length_{};
  /// Transition length to use for flash transitions.
  uint32_t flash_transition_length_{};
  /// Gamma correction factor for the light.
  float gamma_correct_{};
  /// Restore mode of the light.
  LightRestoreMode restore_mode_;
  /// List of effects for this light.
  std::vector<LightEffect *> effects_;
};

}  // namespace light
}  // namespace esphome
