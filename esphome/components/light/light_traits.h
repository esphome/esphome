#pragma once

#include "color_mode.h"
#include <set>

namespace esphome {
namespace light {

/// This class is used to represent the capabilities of a light.
class LightTraits {
 public:
  LightTraits() = default;

  bool get_supports_brightness() const { return this->supports_brightness_; }
  void set_supports_brightness(bool supports_brightness) { this->supports_brightness_ = supports_brightness; }

  std::set<ColorMode> get_supported_color_modes() const { return this->supported_color_modes_; }
  void set_supported_color_modes(std::set<ColorMode> supported_color_modes) {
    this->supported_color_modes_ = std::move(supported_color_modes);
  }

  bool supports_color_mode(ColorMode color_mode) const { return this->supported_color_modes_.count(color_mode); }
  bool supports_color_channel(ColorChannel color_channel) const {
    for (auto mode : this->supported_color_modes_) {
      if (*mode & *color_channel)
        return true;
    }
    return false;
  }

  bool get_supports_rgb() const { return this->supports_rgb_; }
  void set_supports_rgb(bool supports_rgb) { this->supports_rgb_ = supports_rgb; }
  bool get_supports_rgb_white_value() const { return this->supports_rgb_white_value_; }
  void set_supports_rgb_white_value(bool supports_rgb_white_value) {
    this->supports_rgb_white_value_ = supports_rgb_white_value;
  }
  bool get_supports_color_temperature() const { return this->supports_color_temperature_; }
  void set_supports_color_temperature(bool supports_color_temperature) {
    this->supports_color_temperature_ = supports_color_temperature;
  }
  bool get_supports_color_interlock() const { return this->supports_color_interlock_; }
  void set_supports_color_interlock(bool supports_color_interlock) {
    this->supports_color_interlock_ = supports_color_interlock;
  }
  float get_min_mireds() const { return this->min_mireds_; }
  void set_min_mireds(float min_mireds) { this->min_mireds_ = min_mireds; }
  float get_max_mireds() const { return this->max_mireds_; }
  void set_max_mireds(float max_mireds) { this->max_mireds_ = max_mireds; }

 protected:
  bool supports_brightness_{false};
  std::set<ColorMode> supported_color_modes_{};
  bool supports_rgb_{false};
  bool supports_rgb_white_value_{false};
  bool supports_color_temperature_{false};
  float min_mireds_{0};
  float max_mireds_{0};
  bool supports_color_interlock_{false};
};

}  // namespace light
}  // namespace esphome
