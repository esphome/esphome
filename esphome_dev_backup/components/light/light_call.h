#pragma once

#include "esphome/core/optional.h"
#include "light_color_values.h"
#include <set>

namespace esphome {
namespace light {

class LightState;

/** This class represents a requested change in a light state.
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
  LightCall &from_light_color_values(const LightColorValues &values);

  void perform();

 protected:
  /// Get the currently targeted, or active if none set, color mode.
  ColorMode get_active_color_mode_();

  /// Validate all properties and return the target light color values.
  LightColorValues validate_();

  //// Compute the color mode that should be used for this call.
  ColorMode compute_color_mode_();
  /// Get potential color modes for this light call.
  std::set<ColorMode> get_suitable_color_modes_();
  /// Some color modes also can be set using non-native parameters, transform those calls.
  void transform_parameters_();

  bool has_transition_() { return this->transition_length_.has_value(); }
  bool has_flash_() { return this->flash_length_.has_value(); }
  bool has_effect_() { return this->effect_.has_value(); }

  LightState *parent_;
  optional<bool> state_;
  optional<uint32_t> transition_length_;
  optional<uint32_t> flash_length_;
  optional<ColorMode> color_mode_;
  optional<float> brightness_;
  optional<float> color_brightness_;
  optional<float> red_;
  optional<float> green_;
  optional<float> blue_;
  optional<float> white_;
  optional<float> color_temperature_;
  optional<float> cold_white_;
  optional<float> warm_white_;
  optional<uint32_t> effect_;
  bool publish_{true};
  bool save_{true};
};

}  // namespace light
}  // namespace esphome
