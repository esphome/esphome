#pragma once

#include "color_mode.h"
#include "esphome/core/helpers.h"

namespace esphome {

#ifdef USE_API
namespace api {
class APIConnection;
}  // namespace api
#endif

namespace light {

/// This class is used to represent the capabilities of a light.
class LightTraits {
 public:
  LightTraits() = default;

  // Return by value to avoid dangling reference when get_traits() returns a temporary
  ColorModeMask get_supported_color_modes() const { return this->supported_color_modes_; }
  void set_supported_color_modes(ColorModeMask supported_color_modes) {
    this->supported_color_modes_ = supported_color_modes;
  }
  void set_supported_color_modes(std::initializer_list<ColorMode> modes) {
    this->supported_color_modes_ = ColorModeMask(modes);
  }

  bool supports_color_mode(ColorMode color_mode) const { return this->supported_color_modes_.count(color_mode) > 0; }
  bool supports_color_capability(ColorCapability color_capability) const {
    return has_capability(this->supported_color_modes_, color_capability);
  }

  float get_min_mireds() const { return this->min_mireds_; }
  void set_min_mireds(float min_mireds) { this->min_mireds_ = min_mireds; }
  float get_max_mireds() const { return this->max_mireds_; }
  void set_max_mireds(float max_mireds) { this->max_mireds_ = max_mireds; }

 protected:
  float min_mireds_{0};
  float max_mireds_{0};
  ColorModeMask supported_color_modes_{};
};

}  // namespace light
}  // namespace esphome
