#pragma once

#include "esphome/core/component.h"
#include "esphome/core/optional.h"
#include "esphome/core/preferences.h"
#include "light_effect.h"
#include "light_color_values.h"
#include "light_traits.h"
#include "light_transformer.h"

namespace esphome {
namespace light {

class LightState;
class LightOutput;

class LightCall {
 public:
  explicit LightCall(LightState *parent) : parent_(parent) {}

  /// Set the binary ON/OFF state of the light.
  LightCall &set_state(optional<bool> state);
  /// Set the binary ON/OFF state of the light.
  LightCall &set_state(bool state);
  /** Set the transition length of this call in milliseconds.
   *
   * This argument is ignored for starting flashes and effects.
   *
   * Defaults to the default transition length defined in the light configuration.
   */
  LightCall &set_transition_length(optional<uint32_t> transition_length);
  /** Set the transition length of this call in milliseconds.
   *
   * This argument is ignored for starting flashes and effects.
   *
   * Defaults to the default transition length defined in the light configuration.
   */
  LightCall &set_transition_length(uint32_t transition_length);
  /// Set the transition length property if the light supports transitions.
  LightCall &set_transition_length_if_supported(uint32_t transition_length);
  /// Start and set the flash length of this call in milliseconds.
  LightCall &set_flash_length(optional<uint32_t> flash_length);
  /// Start and set the flash length of this call in milliseconds.
  LightCall &set_flash_length(uint32_t flash_length);
  /// Set the target brightness of the light from 0.0 (fully off) to 1.0 (fully on)
  LightCall &set_brightness(optional<float> brightness);
  /// Set the target brightness of the light from 0.0 (fully off) to 1.0 (fully on)
  LightCall &set_brightness(float brightness);
  /// Set the brightness property if the light supports brightness.
  LightCall &set_brightness_if_supported(float brightness);
  /** Set the red RGB value of the light from 0.0 to 1.0.
   *
   * Note that this only controls the color of the light, not its brightness.
   */
  LightCall &set_red(optional<float> red);
  /** Set the red RGB value of the light from 0.0 to 1.0.
   *
   * Note that this only controls the color of the light, not its brightness.
   */
  LightCall &set_red(float red);
  /// Set the red property if the light supports RGB.
  LightCall &set_red_if_supported(float red);
  /** Set the green RGB value of the light from 0.0 to 1.0.
   *
   * Note that this only controls the color of the light, not its brightness.
   */
  LightCall &set_green(optional<float> green);
  /** Set the green RGB value of the light from 0.0 to 1.0.
   *
   * Note that this only controls the color of the light, not its brightness.
   */
  LightCall &set_green(float green);
  /// Set the green property if the light supports RGB.
  LightCall &set_green_if_supported(float green);
  /** Set the blue RGB value of the light from 0.0 to 1.0.
   *
   * Note that this only controls the color of the light, not its brightness.
   */
  LightCall &set_blue(optional<float> blue);
  /** Set the blue RGB value of the light from 0.0 to 1.0.
   *
   * Note that this only controls the color of the light, not its brightness.
   */
  LightCall &set_blue(float blue);
  /// Set the blue property if the light supports RGB.
  LightCall &set_blue_if_supported(float blue);
  /// Set the white value value of the light from 0.0 to 1.0 for RGBW[W] lights.
  LightCall &set_white(optional<float> white);
  /// Set the white value value of the light from 0.0 to 1.0 for RGBW[W] lights.
  LightCall &set_white(float white);
  /// Set the white property if the light supports RGB.
  LightCall &set_white_if_supported(float white);
  /// Set the color temperature of the light in mireds for CWWW or RGBWW lights.
  LightCall &set_color_temperature(optional<float> color_temperature);
  /// Set the color temperature of the light in mireds for CWWW or RGBWW lights.
  LightCall &set_color_temperature(float color_temperature);
  /// Set the color_temperature property if the light supports color temperature.
  LightCall &set_color_temperature_if_supported(float color_temperature);
  /// Set the effect of the light by its name.
  LightCall &set_effect(optional<std::string> effect);
  /// Set the effect of the light by its name.
  LightCall &set_effect(const std::string &effect);
  /// Set the effect of the light by its internal index number (only for internal use).
  LightCall &set_effect(uint32_t effect_number);
  LightCall &set_effect(optional<uint32_t> effect_number);
  /// Set whether this light call should trigger a publish state.
  LightCall &set_publish(bool publish);
  /// Set whether this light call should trigger a save state to recover them at startup..
  LightCall &set_save(bool save);

  /** Set the RGB color of the light by RGB values.
   *
   * Please note that this only changes the color of the light, not the brightness.
   *
   * @param red The red color value from 0.0 to 1.0.
   * @param green The green color value from 0.0 to 1.0.
   * @param blue The blue color value from 0.0 to 1.0.
   * @return The light call for chaining setters.
   */
  LightCall &set_rgb(float red, float green, float blue);
  /** Set the RGBW color of the light by RGB values.
   *
   * Please note that this only changes the color of the light, not the brightness.
   *
   * @param red The red color value from 0.0 to 1.0.
   * @param green The green color value from 0.0 to 1.0.
   * @param blue The blue color value from 0.0 to 1.0.
   * @param white The white color value from 0.0 to 1.0.
   * @return The light call for chaining setters.
   */
  LightCall &set_rgbw(float red, float green, float blue, float white);
#ifdef USE_JSON
  LightCall &parse_color_json(JsonObject &root);
  LightCall &parse_json(JsonObject &root);
#endif
  LightCall &from_light_color_values(const LightColorValues &values);

  void perform();

 protected:
  /// Validate all properties and return the target light color values.
  LightColorValues validate_();

  bool has_transition_() { return this->transition_length_.has_value(); }
  bool has_flash_() { return this->flash_length_.has_value(); }
  bool has_effect_() { return this->effect_.has_value(); }

  LightState *parent_;
  optional<bool> state_;
  optional<uint32_t> transition_length_;
  optional<uint32_t> flash_length_;
  optional<float> brightness_;
  optional<float> red_;
  optional<float> green_;
  optional<float> blue_;
  optional<float> white_;
  optional<float> color_temperature_;
  optional<uint32_t> effect_;
  bool publish_{true};
  bool save_{true};
};

enum LightRestoreMode {
  LIGHT_RESTORE_DEFAULT_OFF,
  LIGHT_RESTORE_DEFAULT_ON,
  LIGHT_ALWAYS_OFF,
  LIGHT_ALWAYS_ON,
};

/** This class represents the communication layer between the front-end MQTT layer and the
 * hardware output layer.
 */
class LightState : public Nameable, public Component {
 public:
  /// Construct this LightState using the provided traits and name.
  LightState(const std::string &name, LightOutput *output);

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
   * This property is read-only for users. Any changes to it will be ignored.
   */
  LightColorValues current_values;

  /// Deprecated method to access current_values.
  ESPDEPRECATED("get_current_values() is deprecated, please use .current_values instead.")
  LightColorValues get_current_values();

  /// Deprecated method to access remote_values.
  ESPDEPRECATED("get_remote_values() is deprecated, please use .remote_values instead.")
  LightColorValues get_remote_values();

  /** The remote color values reported to the frontend.
   *
   * These are different from the "current" values: For example transitions will
   * continuously change the "current" values. But the remote values will immediately
   * switch to the target value for a transition, reducing the number of packets sent.
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

  /** This lets front-end components subscribe to light change events.
   *
   * This is different from add_new_current_values_callback in that it only sends events for start
   * and end values. For example, with transitions it will only send a single callback whereas
   * the callback passed in add_new_current_values_callback will be called every loop() cycle when
   * a transition is active
   *
   * Note the callback should get the output values through get_remote_values().
   *
   * @param send_callback The callback.
   */
  void add_new_remote_values_callback(std::function<void()> &&send_callback);

  /// Return whether the light has any effects that meet the trait requirements.
  bool supports_effects();

#ifdef USE_JSON
  /// Dump the state of this light as JSON.
  void dump_json(JsonObject &root);
#endif

  /// Set the default transition length, i.e. the transition length when no transition is provided.
  void set_default_transition_length(uint32_t default_transition_length);

  /// Set the gamma correction factor
  void set_gamma_correct(float gamma_correct);
  float get_gamma_correct() const { return this->gamma_correct_; }
  void set_restore_mode(LightRestoreMode restore_mode) { restore_mode_ = restore_mode; }

  const std::vector<LightEffect *> &get_effects() const;

  void add_effects(std::vector<LightEffect *> effects);

  void current_values_as_binary(bool *binary);

  void current_values_as_brightness(float *brightness);

  void current_values_as_rgb(float *red, float *green, float *blue);

  void current_values_as_rgbw(float *red, float *green, float *blue, float *white);

  void current_values_as_rgbww(float *red, float *green, float *blue, float *cold_white, float *warm_white);

  void current_values_as_cwww(float *cold_white, float *warm_white);

 protected:
  friend LightOutput;
  friend LightCall;
  friend class AddressableLight;

  uint32_t hash_base() override;

  /// Internal method to start an effect with the given index
  void start_effect_(uint32_t effect_index);
  /// Internal method to stop the current effect (if one is active).
  void stop_effect_();
  /// Internal method to start a transition to the target color with the given length.
  void start_transition_(const LightColorValues &target, uint32_t length);

  /// Internal method to start a flash for the specified amount of time.
  void start_flash_(const LightColorValues &target, uint32_t length);

  /// Internal method to set the color values to target immediately (with no transition).
  void set_immediately_(const LightColorValues &target, bool set_remote_values);

  /// Internal method to start a transformer.
  void set_transformer_(std::unique_ptr<LightTransformer> transformer);

  LightEffect *get_active_effect_();

  /// Object used to store the persisted values of the light.
  ESPPreferenceObject rtc_;
  /// Restore mode of the light.
  LightRestoreMode restore_mode_;
  /// Default transition length for all transitions in ms.
  uint32_t default_transition_length_{};
  /// Value for storing the index of the currently active effect. 0 if no effect is active
  uint32_t active_effect_index_{};
  /// The currently active transformer for this light (transition/flash).
  std::unique_ptr<LightTransformer> transformer_{nullptr};
  /** Callback to call when new values for the frontend are available.
   *
   * "Remote values" are light color values that are reported to the frontend and have a lower
   * publish frequency than the "real" color values. For example, during transitions the current
   * color value may change continuously, but the remote values will be reported as the target values
   * starting with the beginning of the transition.
   */
  CallbackManager<void()> remote_values_callback_{};
  LightOutput *output_;  ///< Store the output to allow effects to have more access.
  /// Whether the light value should be written in the next cycle.
  bool next_write_{true};
  /// Gamma correction factor for the light.
  float gamma_correct_{};
  /// List of effects for this light.
  std::vector<LightEffect *> effects_;
};

}  // namespace light
}  // namespace esphome
