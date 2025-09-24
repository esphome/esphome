#pragma once

#include "esphome/core/helpers.h"
#include "color_mode.h"
#include <cmath>

namespace esphome {
namespace light {

inline static uint8_t to_uint8_scale(float x) { return static_cast<uint8_t>(roundf(x * 255.0f)); }

/** This class represents the color state for a light object.
 *
 * The representation of the color state is dependent on the active color mode. A color mode consists of multiple
 * color capabilities, and each color capability has its own representation in this class. The fields available are as
 * follows:
 *
 * Always:
 * - color_mode: The currently active color mode.
 *
 * For ON_OFF capability:
 * - state: Whether the light should be on/off. Represented as a float for transitions.
 *
 * For BRIGHTNESS capability:
 * - brightness: The master brightness of the light, should be applied to all channels.
 *
 * For RGB capability:
 * - color_brightness: The brightness of the color channels of the light.
 * - red, green, blue: The RGB values of the current color. They are normalized, so at least one of them is always 1.0.
 *
 * For WHITE capability:
 * - white: The brightness of the white channel of the light.
 *
 * For COLOR_TEMPERATURE capability:
 * - color_temperature: The color temperature of the white channel in mireds. Note that it is not clamped to the valid
 *   range as set in the traits, so the output needs to do this.
 *
 * For COLD_WARM_WHITE capability:
 * - cold_white, warm_white: The brightness of the light's cold and warm white channels.
 *
 * All values (except color temperature) are represented using floats in the range 0.0 (off) to 1.0 (on), and are
 * automatically clamped to this range. Properties not used in the current color mode can still have (invalid) values
 * and must not be accessed by the light output.
 */
class LightColorValues {
 public:
  /// Construct the LightColorValues with all attributes enabled, but state set to off.
  LightColorValues()
      : color_mode_(ColorMode::UNKNOWN),
        state_(0.0f),
        brightness_(1.0f),
        color_brightness_(1.0f),
        red_(1.0f),
        green_(1.0f),
        blue_(1.0f),
        white_(1.0f),
        color_temperature_{0.0f},
        cold_white_{1.0f},
        warm_white_{1.0f} {}

  LightColorValues(ColorMode color_mode, float state, float brightness, float color_brightness, float red, float green,
                   float blue, float white, float color_temperature, float cold_white, float warm_white) {
    this->set_color_mode(color_mode);
    this->set_state(state);
    this->set_brightness(brightness);
    this->set_color_brightness(color_brightness);
    this->set_red(red);
    this->set_green(green);
    this->set_blue(blue);
    this->set_white(white);
    this->set_color_temperature(color_temperature);
    this->set_cold_white(cold_white);
    this->set_warm_white(warm_white);
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
    v.set_color_mode(end.color_mode_);
    v.set_state(esphome::lerp(completion, start.get_state(), end.get_state()));
    v.set_brightness(esphome::lerp(completion, start.get_brightness(), end.get_brightness()));
    v.set_color_brightness(esphome::lerp(completion, start.get_color_brightness(), end.get_color_brightness()));
    v.set_red(esphome::lerp(completion, start.get_red(), end.get_red()));
    v.set_green(esphome::lerp(completion, start.get_green(), end.get_green()));
    v.set_blue(esphome::lerp(completion, start.get_blue(), end.get_blue()));
    v.set_white(esphome::lerp(completion, start.get_white(), end.get_white()));
    v.set_color_temperature(esphome::lerp(completion, start.get_color_temperature(), end.get_color_temperature()));
    v.set_cold_white(esphome::lerp(completion, start.get_cold_white(), end.get_cold_white()));
    v.set_warm_white(esphome::lerp(completion, start.get_warm_white(), end.get_warm_white()));
    return v;
  }

  /** Normalize the color (RGB/W) component.
   *
   * Divides all color attributes by the maximum attribute, so effectively set at least one attribute to 1.
   * For example: r=0.3, g=0.5, b=0.4 => r=0.6, g=1.0, b=0.8.
   *
   * Note that this does NOT retain the brightness information from the color attributes.
   *
   * @param traits Used for determining which attributes to consider.
   */
  void normalize_color() {
    if (this->color_mode_ & ColorCapability::RGB) {
      float max_value = fmaxf(this->get_red(), fmaxf(this->get_green(), this->get_blue()));
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
  }

  // Note that method signature of as_* methods is kept as-is for compatibility reasons, so not all parameters
  // are always used or necessary. Methods will be deprecated later.

  /// Convert these light color values to a binary representation and write them to binary.
  void as_binary(bool *binary) const { *binary = this->state_ == 1.0f; }

  /// Convert these light color values to a brightness-only representation and write them to brightness.
  void as_brightness(float *brightness, float gamma = 0) const {
    *brightness = gamma_correct(this->state_ * this->brightness_, gamma);
  }

  /// Convert these light color values to an RGB representation and write them to red, green, blue.
  void as_rgb(float *red, float *green, float *blue, float gamma = 0, bool color_interlock = false) const {
    if (this->color_mode_ & ColorCapability::RGB) {
      float brightness = this->state_ * this->brightness_ * this->color_brightness_;
      *red = gamma_correct(brightness * this->red_, gamma);
      *green = gamma_correct(brightness * this->green_, gamma);
      *blue = gamma_correct(brightness * this->blue_, gamma);
    } else {
      *red = *green = *blue = 0;
    }
  }

  /// Convert these light color values to an RGBW representation and write them to red, green, blue, white.
  void as_rgbw(float *red, float *green, float *blue, float *white, float gamma = 0,
               bool color_interlock = false) const {
    this->as_rgb(red, green, blue, gamma);
    if (this->color_mode_ & ColorCapability::WHITE) {
      *white = gamma_correct(this->state_ * this->brightness_ * this->white_, gamma);
    } else {
      *white = 0;
    }
  }

  /// Convert these light color values to an RGBWW representation with the given parameters.
  void as_rgbww(float *red, float *green, float *blue, float *cold_white, float *warm_white, float gamma = 0,
                bool constant_brightness = false) const {
    this->as_rgb(red, green, blue, gamma);
    this->as_cwww(cold_white, warm_white, gamma, constant_brightness);
  }

  /// Convert these light color values to an RGB+CT+BR representation with the given parameters.
  void as_rgbct(float color_temperature_cw, float color_temperature_ww, float *red, float *green, float *blue,
                float *color_temperature, float *white_brightness, float gamma = 0) const {
    this->as_rgb(red, green, blue, gamma);
    this->as_ct(color_temperature_cw, color_temperature_ww, color_temperature, white_brightness, gamma);
  }

  /// Convert these light color values to an CWWW representation with the given parameters.
  void as_cwww(float *cold_white, float *warm_white, float gamma = 0, bool constant_brightness = false) const {
    if (this->color_mode_ & ColorCapability::COLD_WARM_WHITE) {
      const float cw_level = gamma_correct(this->cold_white_, gamma);
      const float ww_level = gamma_correct(this->warm_white_, gamma);
      const float white_level = gamma_correct(this->state_ * this->brightness_, gamma);
      if (!constant_brightness) {
        *cold_white = white_level * cw_level;
        *warm_white = white_level * ww_level;
      } else {
        // Just multiplying by cw_level / (cw_level + ww_level) would divide out the brightness information from the
        // cold_white and warm_white settings (i.e. cw=0.8, ww=0.4 would be identical to cw=0.4, ww=0.2), which breaks
        // transitions. Use the highest value as the brightness for the white channels (the alternative, using cw+ww/2,
        // reduces to cw/2 and ww/2, which would still limit brightness to 100% of a single channel, but isn't very
        // useful in all other aspects -- that behaviour can also be achieved by limiting the output power).
        const float sum = cw_level > 0 || ww_level > 0 ? cw_level + ww_level : 1;  // Don't divide by zero.
        *cold_white = white_level * std::max(cw_level, ww_level) * cw_level / sum;
        *warm_white = white_level * std::max(cw_level, ww_level) * ww_level / sum;
      }
    } else {
      *cold_white = *warm_white = 0;
    }
  }

  /// Convert these light color values to a CT+BR representation with the given parameters.
  void as_ct(float color_temperature_cw, float color_temperature_ww, float *color_temperature, float *white_brightness,
             float gamma = 0) const {
    const float white_level = this->color_mode_ & ColorCapability::RGB ? this->white_ : 1;
    if (this->color_mode_ & ColorCapability::COLOR_TEMPERATURE) {
      *color_temperature =
          (this->color_temperature_ - color_temperature_cw) / (color_temperature_ww - color_temperature_cw);
      *white_brightness = gamma_correct(this->state_ * this->brightness_ * white_level, gamma);
    } else {  // Probably won't get here but put this here anyway.
      *white_brightness = 0;
    }
  }

  /// Compare this LightColorValues to rhs, return true if and only if all attributes match.
  bool operator==(const LightColorValues &rhs) const {
    return color_mode_ == rhs.color_mode_ && state_ == rhs.state_ && brightness_ == rhs.brightness_ &&
           color_brightness_ == rhs.color_brightness_ && red_ == rhs.red_ && green_ == rhs.green_ &&
           blue_ == rhs.blue_ && white_ == rhs.white_ && color_temperature_ == rhs.color_temperature_ &&
           cold_white_ == rhs.cold_white_ && warm_white_ == rhs.warm_white_;
  }
  bool operator!=(const LightColorValues &rhs) const { return !(rhs == *this); }

  /// Get the color mode of these light color values.
  ColorMode get_color_mode() const { return this->color_mode_; }
  /// Set the color mode of these light color values.
  void set_color_mode(ColorMode color_mode) { this->color_mode_ = color_mode; }

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

  /// Get the color brightness property of these light color values. In range 0.0 to 1.0
  float get_color_brightness() const { return this->color_brightness_; }
  /// Set the color brightness property of these light color values. In range 0.0 to 1.0
  void set_color_brightness(float brightness) { this->color_brightness_ = clamp(brightness, 0.0f, 1.0f); }

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
  void set_color_temperature(float color_temperature) { this->color_temperature_ = color_temperature; }

  /// Get the color temperature property of these light color values in kelvin.
  float get_color_temperature_kelvin() const {
    if (this->color_temperature_ <= 0) {
      return this->color_temperature_;
    }
    return 1000000.0 / this->color_temperature_;
  }
  /// Set the color temperature property of these light color values in kelvin.
  void set_color_temperature_kelvin(float color_temperature) {
    if (color_temperature <= 0) {
      return;
    }
    this->color_temperature_ = 1000000.0 / color_temperature;
  }

  /// Get the cold white property of these light color values. In range 0.0 to 1.0.
  float get_cold_white() const { return this->cold_white_; }
  /// Set the cold white property of these light color values. In range 0.0 to 1.0.
  void set_cold_white(float cold_white) { this->cold_white_ = clamp(cold_white, 0.0f, 1.0f); }

  /// Get the warm white property of these light color values. In range 0.0 to 1.0.
  float get_warm_white() const { return this->warm_white_; }
  /// Set the warm white property of these light color values. In range 0.0 to 1.0.
  void set_warm_white(float warm_white) { this->warm_white_ = clamp(warm_white, 0.0f, 1.0f); }

 protected:
  ColorMode color_mode_;
  float state_;  ///< ON / OFF, float for transition
  float brightness_;
  float color_brightness_;
  float red_;
  float green_;
  float blue_;
  float white_;
  float color_temperature_;  ///< Color Temperature in Mired
  float cold_white_;
  float warm_white_;
};

}  // namespace light
}  // namespace esphome
