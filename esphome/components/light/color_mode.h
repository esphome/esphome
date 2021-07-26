#pragma once

#include <cstdint>

namespace esphome {
namespace light {

/// Color capabilities are the various outputs that a light has and that can be independently controlled by the user.
enum class ColorCapability : uint8_t {
  /// Light can be turned on/off.
  ON_OFF = 1 << 0,
  /// Master brightness of the light can be controlled.
  BRIGHTNESS = 1 << 1,
  /// Brightness of white channel can be controlled separately from other channels.
  WHITE = 1 << 2,
  /// Color temperature can be controlled.
  COLOR_TEMPERATURE = 1 << 3,
  /// Brightness of cold and warm white output can be controlled.
  COLD_WARM_WHITE = 1 << 4,
  /// Color can be controlled using RGB format (includes a brightness control for the color).
  RGB = 1 << 5
};

// Define unary * operator to convert enum to underlying value -- this saves a lot of typing and ugly code.
inline constexpr uint8_t operator*(ColorCapability val) { return static_cast<uint8_t>(val); }

/// Color modes are a combination of color capabilities that can be used at the same time.
enum class ColorMode : uint8_t {
  /// No color mode configured (cannot be a supported mode, only active when light is off).
  UNKNOWN = 0,
  /// Only on/off control.
  ON_OFF = *ColorCapability::ON_OFF,
  /// Dimmable light.
  BRIGHTNESS = *ColorCapability::BRIGHTNESS,
  /// White output only (use only if the light also has another color mode such as RGB).
  WHITE = *ColorCapability::ON_OFF | *ColorCapability::BRIGHTNESS | *ColorCapability::WHITE,
  /// Controllable color temperature output.
  COLOR_TEMPERATURE = *ColorCapability::ON_OFF | *ColorCapability::BRIGHTNESS | *ColorCapability::COLOR_TEMPERATURE,
  /// Cold and warm white output with individually controllable brightness.
  COLD_WARM_WHITE = *ColorCapability::ON_OFF | *ColorCapability::BRIGHTNESS | *ColorCapability::COLD_WARM_WHITE,
  /// RGB color output.
  RGB = *ColorCapability::ON_OFF | *ColorCapability::BRIGHTNESS | *ColorCapability::RGB,
  /// RGB color output and a separate white output.
  RGB_WHITE = *ColorCapability::ON_OFF | *ColorCapability::BRIGHTNESS | *ColorCapability::RGB | *ColorCapability::WHITE,
  /// RGB color output and a separate white output with controllable color temperature.
  RGB_COLOR_TEMPERATURE = *ColorCapability::ON_OFF | *ColorCapability::BRIGHTNESS | *ColorCapability::RGB |
                          *ColorCapability::WHITE | *ColorCapability::COLOR_TEMPERATURE,
  /// RGB color output, and separate cold and warm white outputs.
  RGB_COLD_WARM_WHITE = *ColorCapability::ON_OFF | *ColorCapability::BRIGHTNESS | *ColorCapability::RGB |
                        *ColorCapability::COLD_WARM_WHITE
};

// Define unary * operator to convert enum to underlying value -- this saves a lot of typing and ugly code.
inline constexpr uint8_t operator*(ColorMode val) { return static_cast<uint8_t>(val); }

}  // namespace light
}  // namespace esphome
