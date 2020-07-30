#pragma once

#include "esphome/core/helpers.h"
#include "esphome/core/defines.h"
#include "light_traits.h"

#ifdef USE_JSON
#include "esphome/components/json/json_util.h"
#endif

namespace esphome {
namespace light {

/** This class represents the color state for a light object.
 *
 * All values in this class are represented using floats in the range from 0.0 (off) to 1.0 (on).
 * Not all values have to be populated though, for example a simple monochromatic light only needs
 * to access the state and brightness attributes.
 *
 * Please note all float values are automatically clamped.
 *
 * state - Whether the light should be on/off. Represented as a float for transitions.
 * brightness - The brightness of the light.
 * red, green, blue - RGB values.
 * white - The white value for RGBW lights.
 * color_temperature - Temperature of the white value, range from 0.0 (cold) to 1.0 (warm)
 */
class LightColorValues {
 public:
  /// Construct the LightColorValues with all attributes enabled, but state set to 0.0
  LightColorValues()
      : state_(0.0f),
        brightness_(1.0f),
        red_(1.0f),
        green_(1.0f),
        blue_(1.0f),
        white_(1.0f),
        color_temperature_{1.0f} {}

  LightColorValues(float state, float brightness, float red, float green, float blue, float white,
                   float color_temperature = 1.0f) {
    this->set_state(state);
    this->set_brightness(brightness);
    this->set_red(red);
    this->set_green(green);
    this->set_blue(blue);
    this->set_white(white);
    this->set_color_temperature(color_temperature);
  }

  LightColorValues(bool state, float brightness, float red, float green, float blue, float white,
                   float color_temperature = 1.0f)
      : LightColorValues(state ? 1.0f : 0.0f, brightness, red, green, blue, white, color_temperature) {}

  /// Create light color values from a binary true/false state.
  static LightColorValues from_binary(bool state) { return {state, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}; }

  /// Create light color values from a monochromatic brightness state.
  static LightColorValues from_monochromatic(float brightness) {
    if (brightness == 0.0f)
      return {0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    else
      return {1.0f, brightness, 1.0f, 1.0f, 1.0f, 1.0f};
  }

  /// Create light color values from an RGB state.
  static LightColorValues from_rgb(float r, float g, float b) {
    float brightness = std::max(r, std::max(g, b));
    if (brightness == 0.0f) {
      return {0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    } else {
      return {1.0f, brightness, r / brightness, g / brightness, b / brightness, 1.0f};
    }
  }

  /// Create light color values from an RGBW state.
  static LightColorValues from_rgbw(float r, float g, float b, float w) {
    float brightness = std::max(r, std::max(g, std::max(b, w)));
    if (brightness == 0.0f) {
      return {0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    } else {
      return {1.0f, brightness, r / brightness, g / brightness, b / brightness, w / brightness};
    }
  }

  /** Linearly interpolate between the values in start to the values in end.
   *
   * This function linearly interpolates the color value by just interpolating every attribute
   * independently.
   *
   * @param start The interpolation start values.
   * @param end The interpolation end values.
   * @param completion The completion value. 0 -> start, 1 -> end.
   * @return The linearly interpolated LightColorValues.
   */
  static LightColorValues lerp(const LightColorValues &start, const LightColorValues &end, float completion) {
    LightColorValues v;
    v.set_state(esphome::lerp(completion, start.get_state(), end.get_state()));
    v.set_brightness(esphome::lerp(completion, start.get_brightness(), end.get_brightness()));
    v.set_red(esphome::lerp(completion, start.get_red(), end.get_red()));
    v.set_green(esphome::lerp(completion, start.get_green(), end.get_green()));
    v.set_blue(esphome::lerp(completion, start.get_blue(), end.get_blue()));
    v.set_white(esphome::lerp(completion, start.get_white(), end.get_white()));
    v.set_color_temperature(esphome::lerp(completion, start.get_color_temperature(), end.get_color_temperature()));
    return v;
  }

#ifdef USE_JSON
  /** Dump this color into a JsonObject. Only dumps values if the corresponding traits are marked supported by traits.
   *
   * @param root The json root object.
   * @param traits The traits object used for determining whether to include certain attributes.
   */
  void dump_json(JsonObject &root, const LightTraits &traits) const {
    root["state"] = (this->get_state() != 0.0f) ? "ON" : "OFF";
    if (traits.get_supports_brightness())
      root["brightness"] = uint8_t(this->get_brightness() * 255);
    if (traits.get_supports_rgb()) {
      JsonObject &color = root.createNestedObject("color");
      color["r"] = uint8_t(this->get_red() * 255);
      color["g"] = uint8_t(this->get_green() * 255);
      color["b"] = uint8_t(this->get_blue() * 255);
    }
    if (traits.get_supports_rgb_white_value())
      root["white_value"] = uint8_t(this->get_white() * 255);
    if (traits.get_supports_color_temperature())
      root["color_temp"] = uint32_t(this->get_color_temperature());
  }
#endif

  /** Normalize the color (RGB/W) component.
   *
   * Divides all color attributes by the maximum attribute, so effectively set at least one attribute to 1.
   * For example: r=0.3, g=0.5, b=0.4 => r=0.6, g=1.0, b=0.8
   *
   * @param traits Used for determining which attributes to consider.
   */
  void normalize_color(const LightTraits &traits) {
    if (traits.get_supports_rgb()) {
      float max_value = fmaxf(this->get_red(), fmaxf(this->get_green(), this->get_blue()));
      if (traits.get_supports_rgb_white_value()) {
        max_value = fmaxf(max_value, this->get_white());
        if (max_value == 0.0f) {
          this->set_white(1.0f);
        } else {
          this->set_white(this->get_white() / max_value);
        }
      }
      if (max_value == 0.0f) {
        this->set_red(1.0f);
        this->set_green(1.0f);
        this->set_blue(1.0f);
      } else {
        this->set_red(this->get_red() / max_value);
        this->set_green(this->get_green() / max_value);
        this->set_blue(this->get_blue() / max_value);
      }
    }

    if (traits.get_supports_brightness() && this->get_brightness() == 0.0f) {
      if (traits.get_supports_rgb_white_value()) {
        // 0% brightness for RGBW[W] means no RGB channel, but white channel on.
        // do nothing
      } else {
        // 0% brightness means off
        this->set_state(false);
        // reset brightness to 100%
        this->set_brightness(1.0f);
      }
    }
  }

  /// Convert these light color values to a binary representation and write them to binary.
  void as_binary(bool *binary) const { *binary = this->state_ == 1.0f; }

  /// Convert these light color values to a brightness-only representation and write them to brightness.
  void as_brightness(float *brightness, float gamma = 0) const {
    *brightness = gamma_correct(this->state_ * this->brightness_, gamma);
  }

  /// Convert these light color values to an RGB representation and write them to red, green, blue.
  void as_rgb(float *red, float *green, float *blue, float gamma = 0, bool color_interlock = false) const {
    float brightness = this->state_ * this->brightness_;
    if (color_interlock) {
      brightness = brightness * (1.0f - this->white_);
    }
    *red = gamma_correct(brightness * this->red_, gamma);
    *green = gamma_correct(brightness * this->green_, gamma);
    *blue = gamma_correct(brightness * this->blue_, gamma);
  }

  /// Convert these light color values to an RGBW representation and write them to red, green, blue, white.
  void as_rgbw(float *red, float *green, float *blue, float *white, float gamma = 0,
               bool color_interlock = false) const {
    this->as_rgb(red, green, blue, gamma, color_interlock);
    *white = gamma_correct(this->state_ * this->brightness_ * this->white_, gamma);
  }

  /// Convert these light color values to an RGBWW representation with the given parameters.
  void as_rgbww(float color_temperature_cw, float color_temperature_ww, float *red, float *green, float *blue,
                float *cold_white, float *warm_white, float gamma = 0, bool constant_brightness = false,
                bool color_interlock = false) const {
    this->as_rgb(red, green, blue, gamma, color_interlock);
    const float color_temp = clamp(this->color_temperature_, color_temperature_cw, color_temperature_ww);
    const float ww_fraction = (color_temp - color_temperature_cw) / (color_temperature_ww - color_temperature_cw);
    const float cw_fraction = 1.0f - ww_fraction;
    const float white_level = gamma_correct(this->state_ * this->brightness_ * this->white_, gamma);
    *cold_white = white_level * cw_fraction;
    *warm_white = white_level * ww_fraction;
    if (!constant_brightness) {
      const float max_cw_ww = std::max(ww_fraction, cw_fraction);
      *cold_white /= max_cw_ww;
      *warm_white /= max_cw_ww;
    }
  }

  /// Convert these light color values to an CWWW representation with the given parameters.
  void as_cwww(float color_temperature_cw, float color_temperature_ww, float *cold_white, float *warm_white,
               float gamma = 0, bool constant_brightness = false) const {
    const float color_temp = clamp(this->color_temperature_, color_temperature_cw, color_temperature_ww);
    const float ww_fraction = (color_temp - color_temperature_cw) / (color_temperature_ww - color_temperature_cw);
    const float cw_fraction = 1.0f - ww_fraction;
    const float white_level = gamma_correct(this->state_ * this->brightness_ * this->white_, gamma);
    *cold_white = white_level * cw_fraction;
    *warm_white = white_level * ww_fraction;
    if (!constant_brightness) {
      const float max_cw_ww = std::max(ww_fraction, cw_fraction);
      *cold_white /= max_cw_ww;
      *warm_white /= max_cw_ww;
    }
  }

  /// Compare this LightColorValues to rhs, return true if and only if all attributes match.
  bool operator==(const LightColorValues &rhs) const {
    return state_ == rhs.state_ && brightness_ == rhs.brightness_ && red_ == rhs.red_ && green_ == rhs.green_ &&
           blue_ == rhs.blue_ && white_ == rhs.white_ && color_temperature_ == rhs.color_temperature_;
  }
  bool operator!=(const LightColorValues &rhs) const { return !(rhs == *this); }

  /// Get the state of these light color values. In range from 0.0 (off) to 1.0 (on)
  float get_state() const { return this->state_; }
  /// Get the binary true/false state of these light color values.
  bool is_on() const { return this->get_state() != 0.0f; }
  /// Set the state of these light color values. In range from 0.0 (off) to 1.0 (on)
  void set_state(float state) { this->state_ = clamp(state, 0.0f, 1.0f); }
  /// Set the state of these light color values as a binary true/false.
  void set_state(bool state) { this->state_ = state ? 1.0f : 0.0f; }

  /// Get the brightness property of these light color values. In range 0.0 to 1.0
  float get_brightness() const { return this->brightness_; }
  /// Set the brightness property of these light color values. In range 0.0 to 1.0
  void set_brightness(float brightness) { this->brightness_ = clamp(brightness, 0.0f, 1.0f); }

  /// Get the red property of these light color values. In range 0.0 to 1.0
  float get_red() const { return this->red_; }
  /// Set the red property of these light color values. In range 0.0 to 1.0
  void set_red(float red) { this->red_ = clamp(red, 0.0f, 1.0f); }

  /// Get the green property of these light color values. In range 0.0 to 1.0
  float get_green() const { return this->green_; }
  /// Set the green property of these light color values. In range 0.0 to 1.0
  void set_green(float green) { this->green_ = clamp(green, 0.0f, 1.0f); }

  /// Get the blue property of these light color values. In range 0.0 to 1.0
  float get_blue() const { return this->blue_; }
  /// Set the blue property of these light color values. In range 0.0 to 1.0
  void set_blue(float blue) { this->blue_ = clamp(blue, 0.0f, 1.0f); }

  /// Get the white property of these light color values. In range 0.0 to 1.0
  float get_white() const { return white_; }
  /// Set the white property of these light color values. In range 0.0 to 1.0
  void set_white(float white) { this->white_ = clamp(white, 0.0f, 1.0f); }

  /// Get the color temperature property of these light color values in mired.
  float get_color_temperature() const { return this->color_temperature_; }
  /// Set the color temperature property of these light color values in mired.
  void set_color_temperature(float color_temperature) {
    this->color_temperature_ = std::max(0.000001f, color_temperature);
  }

 protected:
  float state_;  ///< ON / OFF, float for transition
  float brightness_;
  float red_;
  float green_;
  float blue_;
  float white_;
  float color_temperature_;  ///< Color Temperature in Mired
};

}  // namespace light
}  // namespace esphome
