#pragma once

#include "esphome/core/helpers.h"
#include "color_mode.h"
#include <set>

namespace esphome {
namespace light {

/// This class is used to represent the capabilities of a light.
class LightTraits {
 public:
  LightTraits() = default;

  const std::set<ColorMode> &get_supported_color_modes() const { return this->supported_color_modes_; }
  void set_supported_color_modes(std::set<ColorMode> supported_color_modes) {
    this->supported_color_modes_ = std::move(supported_color_modes);
  }

  bool supports_color_mode(ColorMode color_mode) const { return this->supported_color_modes_.count(color_mode); }
  bool supports_color_capability(ColorCapability color_capability) const {
    for (auto mode : this->supported_color_modes_) {
      if (mode & color_capability)
        return true;
    }
    return false;
  }

  ESPDEPRECATED("get_supports_brightness() is deprecated, use color modes instead.", "v1.21")
  bool get_supports_brightness() const { return this->supports_color_capability(ColorCapability::BRIGHTNESS); }
  ESPDEPRECATED("get_supports_rgb() is deprecated, use color modes instead.", "v1.21")
  bool get_supports_rgb() const { return this->supports_color_capability(ColorCapability::RGB); }
  ESPDEPRECATED("get_supports_rgb_white_value() is deprecated, use color modes instead.", "v1.21")
  bool get_supports_rgb_white_value() const {
    return this->supports_color_mode(ColorMode::RGB_WHITE) ||
           this->supports_color_mode(ColorMode::RGB_COLOR_TEMPERATURE);
  }
  ESPDEPRECATED("get_supports_color_temperature() is deprecated, use color modes instead.", "v1.21")
  bool get_supports_color_temperature() const {
    return this->supports_color_capability(ColorCapability::COLOR_TEMPERATURE);
  }
  ESPDEPRECATED("get_supports_color_interlock() is deprecated, use color modes instead.", "v1.21")
  bool get_supports_color_interlock() const {
    return this->supports_color_mode(ColorMode::RGB) &&
           (this->supports_color_mode(ColorMode::WHITE) || this->supports_color_mode(ColorMode::COLD_WARM_WHITE) ||
            this->supports_color_mode(ColorMode::COLOR_TEMPERATURE));
  }

  float get_min_mireds() const { return this->min_mireds_; }
  void set_min_mireds(float min_mireds) { this->min_mireds_ = min_mireds; }
  float get_max_mireds() const { return this->max_mireds_; }
  void set_max_mireds(float max_mireds) { this->max_mireds_ = max_mireds; }

 protected:
  std::set<ColorMode> supported_color_modes_{};
  float min_mireds_{0};
  float max_mireds_{0};
};

}  // namespace light
}  // namespace esphome
