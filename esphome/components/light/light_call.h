#pragma once

#include "light_color_values.h"

namespace esphome {

// Forward declaration
struct LogString;

namespace light {

class LightState;

/** This class represents a requested change in a light state.
 *
 * Light state changes are tracked using a bitfield flags_ to minimize memory usage.
 * Each possible light property has a flag indicating whether it has been set.
 * This design keeps LightCall at ~56 bytes to minimize heap fragmentation on
 * ESP8266 and other memory-constrained devices.
 */
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

  /// Set the color mode of the light.
  LightCall &set_color_mode(optional<ColorMode> color_mode);
  /// Set the color mode of the light.
  LightCall &set_color_mode(ColorMode color_mode);
  /// Set the color mode of the light, if this mode is supported.
  LightCall &set_color_mode_if_supported(ColorMode color_mode);

  /// Set the color brightness of the light from 0.0 (no color) to 1.0 (fully on)
  LightCall &set_color_brightness(optional<float> brightness);
  /// Set the color brightness of the light from 0.0 (no color) to 1.0 (fully on)
  LightCall &set_color_brightness(float brightness);
  /// Set the color brightness property if the light supports RGBW.
  LightCall &set_color_brightness_if_supported(float brightness);
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
  /// Set the cold white value of the light from 0.0 to 1.0.
  LightCall &set_cold_white(optional<float> cold_white);
  /// Set the cold white value of the light from 0.0 to 1.0.
  LightCall &set_cold_white(float cold_white);
  /// Set the cold white property if the light supports cold white output.
  LightCall &set_cold_white_if_supported(float cold_white);
  /// Set the warm white value of the light from 0.0 to 1.0.
  LightCall &set_warm_white(optional<float> warm_white);
  /// Set the warm white value of the light from 0.0 to 1.0.
  LightCall &set_warm_white(float warm_white);
  /// Set the warm white property if the light supports cold white output.
  LightCall &set_warm_white_if_supported(float warm_white);
  /// Set the effect of the light by its name.
  LightCall &set_effect(optional<std::string> effect);
  /// Set the effect of the light by its name.
  LightCall &set_effect(const std::string &effect) { return this->set_effect(effect.data(), effect.size()); }
  /// Set the effect of the light by its name and length (zero-copy from API).
  LightCall &set_effect(const char *effect, size_t len);
  /// Set the effect of the light by its internal index number (only for internal use).
  LightCall &set_effect(uint32_t effect_number);
  LightCall &set_effect(optional<uint32_t> effect_number);
  /// Set whether this light call should trigger a publish state.
  LightCall &set_publish(bool publish);
  /// Set whether this light call should trigger a save state to recover them at startup..
  LightCall &set_save(bool save);

  // Getter methods to check if values are set
  bool has_state() const { return (flags_ & FLAG_HAS_STATE) != 0; }
  bool has_brightness() const { return (flags_ & FLAG_HAS_BRIGHTNESS) != 0; }
  bool has_color_brightness() const { return (flags_ & FLAG_HAS_COLOR_BRIGHTNESS) != 0; }
  bool has_red() const { return (flags_ & FLAG_HAS_RED) != 0; }
  bool has_green() const { return (flags_ & FLAG_HAS_GREEN) != 0; }
  bool has_blue() const { return (flags_ & FLAG_HAS_BLUE) != 0; }
  bool has_white() const { return (flags_ & FLAG_HAS_WHITE) != 0; }
  bool has_color_temperature() const { return (flags_ & FLAG_HAS_COLOR_TEMPERATURE) != 0; }
  bool has_cold_white() const { return (flags_ & FLAG_HAS_COLD_WHITE) != 0; }
  bool has_warm_white() const { return (flags_ & FLAG_HAS_WARM_WHITE) != 0; }
  bool has_color_mode() const { return (flags_ & FLAG_HAS_COLOR_MODE) != 0; }

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
  LightCall &from_light_color_values(const LightColorValues &values);

  void perform();

 protected:
  /// Get the currently targeted, or active if none set, color mode.
  ColorMode get_active_color_mode_();

  /// Validate all properties and return the target light color values.
  LightColorValues validate_();

  //// Compute the color mode that should be used for this call.
  ColorMode compute_color_mode_();
  /// Get potential color modes bitmask for this light call.
  color_mode_bitmask_t get_suitable_color_modes_mask_();
  /// Some color modes also can be set using non-native parameters, transform those calls.
  void transform_parameters_();

  // Bitfield flags - each flag indicates whether a corresponding value has been set.
  enum FieldFlags : uint16_t {
    FLAG_HAS_STATE = 1 << 0,
    FLAG_HAS_TRANSITION = 1 << 1,
    FLAG_HAS_FLASH = 1 << 2,
    FLAG_HAS_EFFECT = 1 << 3,
    FLAG_HAS_BRIGHTNESS = 1 << 4,
    FLAG_HAS_COLOR_BRIGHTNESS = 1 << 5,
    FLAG_HAS_RED = 1 << 6,
    FLAG_HAS_GREEN = 1 << 7,
    FLAG_HAS_BLUE = 1 << 8,
    FLAG_HAS_WHITE = 1 << 9,
    FLAG_HAS_COLOR_TEMPERATURE = 1 << 10,
    FLAG_HAS_COLD_WHITE = 1 << 11,
    FLAG_HAS_WARM_WHITE = 1 << 12,
    FLAG_HAS_COLOR_MODE = 1 << 13,
    FLAG_PUBLISH = 1 << 14,
    FLAG_SAVE = 1 << 15,
  };

  inline bool has_transition_() { return (this->flags_ & FLAG_HAS_TRANSITION) != 0; }
  inline bool has_flash_() { return (this->flags_ & FLAG_HAS_FLASH) != 0; }
  inline bool has_effect_() { return (this->flags_ & FLAG_HAS_EFFECT) != 0; }
  inline bool get_publish_() { return (this->flags_ & FLAG_PUBLISH) != 0; }
  inline bool get_save_() { return (this->flags_ & FLAG_SAVE) != 0; }

  // Helper to set flag - defaults to true for common case
  void set_flag_(FieldFlags flag, bool value = true) {
    if (value) {
      this->flags_ |= flag;
    } else {
      this->flags_ &= ~flag;
    }
  }

  // Helper to clear flag - reduces code size for common case
  void clear_flag_(FieldFlags flag) { this->flags_ &= ~flag; }

  // Helper to log unsupported feature and clear flag - reduces code duplication
  void log_and_clear_unsupported_(FieldFlags flag, const LogString *feature, bool use_color_mode_log);

  LightState *parent_;

  // Light state values - use flags_ to check if a value has been set.
  // Group 4-byte aligned members first
  uint32_t transition_length_;
  uint32_t flash_length_;
  uint32_t effect_;
  float brightness_;
  float color_brightness_;
  float red_;
  float green_;
  float blue_;
  float white_;
  float color_temperature_;
  float cold_white_;
  float warm_white_;

  // Smaller members at the end for better packing
  uint16_t flags_{FLAG_PUBLISH | FLAG_SAVE};  // Tracks which values are set
  ColorMode color_mode_;
  bool state_;
};

}  // namespace light
}  // namespace esphome
