#pragma once

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/optional.h"
#include "esphome/core/preferences.h"
#include "esphome/core/string_ref.h"
#include "light_call.h"
#include "light_color_values.h"
#include "light_effect.h"
#include "light_traits.h"
#include "light_transformer.h"

#include "esphome/core/helpers.h"
#include <strings.h>
#include <vector>

namespace esphome::light {

class LightOutput;
class LightState;

/** Listener interface for light remote value changes.
 *
 * Components can implement this interface to receive notifications
 * when the light's remote values change (state, brightness, color, etc.)
 * without the overhead of std::function callbacks.
 */
class LightRemoteValuesListener {
 public:
  virtual void on_light_remote_values_update() = 0;
};

/** Listener interface for light target state reached.
 *
 * Components can implement this interface to receive notifications
 * when the light finishes a transition and reaches its target state
 * without the overhead of std::function callbacks.
 */
class LightTargetStateReachedListener {
 public:
  virtual void on_light_target_state_reached() = 0;
};

enum LightRestoreMode : uint8_t {
  LIGHT_RESTORE_DEFAULT_OFF,
  LIGHT_RESTORE_DEFAULT_ON,
  LIGHT_ALWAYS_OFF,
  LIGHT_ALWAYS_ON,
  LIGHT_RESTORE_INVERTED_DEFAULT_OFF,
  LIGHT_RESTORE_INVERTED_DEFAULT_ON,
  LIGHT_RESTORE_AND_OFF,
  LIGHT_RESTORE_AND_ON,
};

struct LightStateRTCState {
  LightStateRTCState(ColorMode color_mode, bool state, float brightness, float color_brightness, float red, float green,
                     float blue, float white, float color_temp, float cold_white, float warm_white)
      : brightness(brightness),
        color_brightness(color_brightness),
        red(red),
        green(green),
        blue(blue),
        white(white),
        color_temp(color_temp),
        cold_white(cold_white),
        warm_white(warm_white),
        effect(0),
        color_mode(color_mode),
        state(state) {}
  LightStateRTCState() = default;
  // Group 4-byte aligned members first
  float brightness{1.0f};
  float color_brightness{1.0f};
  float red{1.0f};
  float green{1.0f};
  float blue{1.0f};
  float white{1.0f};
  float color_temp{1.0f};
  float cold_white{1.0f};
  float warm_white{1.0f};
  uint32_t effect{0};
  // Group smaller members at the end
  ColorMode color_mode{ColorMode::UNKNOWN};
  bool state{false};
};

/** This class represents the communication layer between the front-end MQTT layer and the
 * hardware output layer.
 */
class LightState : public EntityBase, public Component {
 public:
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
  /// Return the name of the current effect as StringRef (for API usage)
  StringRef get_effect_name_ref();

  /** Add a listener for remote values changes.
   * Listener is notified when the light's remote values change (state, brightness, color, etc.)
   * Lazily allocates the listener vector on first registration.
   */
  void add_remote_values_listener(LightRemoteValuesListener *listener);

  /** Add a listener for target state reached.
   * Listener is notified when the light finishes a transition and reaches its target state.
   * Lazily allocates the listener vector on first registration.
   */
  void add_target_state_reached_listener(LightTargetStateReachedListener *listener);

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

  /// Set the initial state of this light
  void set_initial_state(const LightStateRTCState &initial_state);

  /// Return whether the light has any effects that meet the trait requirements.
  bool supports_effects();

  /// Get all effects for this light state.
  const FixedVector<LightEffect *> &get_effects() const;

  /// Add effects for this light state.
  void add_effects(const std::initializer_list<LightEffect *> &effects);

  /// Get the total number of effects available for this light.
  size_t get_effect_count() const { return this->effects_.size(); }

  /// Get the currently active effect index (0 = no effect, 1+ = effect index).
  uint32_t get_current_effect_index() const { return this->active_effect_index_; }

  /// Get effect index by name. Returns 0 if effect not found.
  uint32_t get_effect_index(const std::string &effect_name) const {
    if (strcasecmp(effect_name.c_str(), "none") == 0) {
      return 0;
    }
    for (size_t i = 0; i < this->effects_.size(); i++) {
      if (strcasecmp(effect_name.c_str(), this->effects_[i]->get_name()) == 0) {
        return i + 1;  // Effects are 1-indexed in active_effect_index_
      }
    }
    return 0;  // Effect not found
  }

  /// Get effect by index. Returns nullptr if index is invalid.
  LightEffect *get_effect_by_index(uint32_t index) const {
    if (index == 0 || index > this->effects_.size()) {
      return nullptr;
    }
    return this->effects_[index - 1];  // Effects are 1-indexed in active_effect_index_
  }

  /// Get effect name by index. Returns "None" for index 0, empty string for invalid index.
  std::string get_effect_name_by_index(uint32_t index) const {
    if (index == 0) {
      return "None";
    }
    if (index > this->effects_.size()) {
      return "";  // Invalid index
    }
    return this->effects_[index - 1]->get_name();
  }

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

  /**
   * Indicator if a transformer (e.g. transition) is active. This is useful
   * for effects e.g. at the start of the apply() method, add a check like:
   *
   * if (this->state_->is_transformer_active()) {
   *   // Something is already running.
   *   return;
   * }
   */
  bool is_transformer_active();

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

  /// Disable loop if neither transformer nor effect is active
  void disable_loop_if_idle_();

  /// Schedule a write to the light output and enable the loop to process it
  void schedule_write_() {
    this->next_write_ = true;
    this->enable_loop();
  }

  /// Store the output to allow effects to have more access.
  LightOutput *output_;
  /// The currently active transformer for this light (transition/flash).
  std::unique_ptr<LightTransformer> transformer_{nullptr};
  /// List of effects for this light.
  FixedVector<LightEffect *> effects_;
  /// Object used to store the persisted values of the light.
  ESPPreferenceObject rtc_;
  /// Value for storing the index of the currently active effect. 0 if no effect is active
  uint32_t active_effect_index_{};
  /// Default transition length for all transitions in ms.
  uint32_t default_transition_length_{};
  /// Transition length to use for flash transitions.
  uint32_t flash_transition_length_{};
  /// Gamma correction factor for the light.
  float gamma_correct_{};
  /// Whether the light value should be written in the next cycle.
  bool next_write_{true};
  // for effects, true if a transformer (transition) is active.
  bool is_transformer_active_ = false;

  /** Listeners for remote values changes.
   *
   * "Remote values" are light color values that are reported to the frontend and have a lower
   * publish frequency than the "real" color values. For example, during transitions the current
   * color value may change continuously, but the remote values will be reported as the target values
   * starting with the beginning of the transition.
   *
   * Lazily allocated - only created when a listener is actually registered.
   */
  std::unique_ptr<std::vector<LightRemoteValuesListener *>> remote_values_listeners_;

  /** Listeners for target state reached.
   * Notified when the state of current_values and remote_values are equal
   * (when the transition is finished).
   *
   * Lazily allocated - only created when a listener is actually registered.
   */
  std::unique_ptr<std::vector<LightTargetStateReachedListener *>> target_state_reached_listeners_;

  /// Initial state of the light.
  optional<LightStateRTCState> initial_state_{};

  /// Restore mode of the light.
  LightRestoreMode restore_mode_;
};

}  // namespace esphome::light
