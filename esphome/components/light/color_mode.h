#pragma once

#include <cstdint>

namespace esphome {
namespace light {

/// Color capabilities are the various outputs that a light has and that can be independently controlled by the user.
enum class ColorCapability : uint8_t {
  /// Intensity of a single white output.
  WHITE = 1,
  /// Color temperature of a single white output.
  COLOR_TEMPERATURE = 2,
  /// Intensities of a cold white and warm white output.
  COLD_WARM_WHITE = 4,
  /// Colored output using RGB format.
  RGB = 8
};

// Define unary * operator to convert enum to underlying value -- this saves a lot of typing and ugly code.
inline constexpr uint8_t operator*(ColorCapability val) { return static_cast<uint8_t>(val); }

/// Color modes are a combination of color capabilities that can be used at the same time.
enum class ColorMode : uint8_t {
  /// No color mode configured (cannot be a supported mode, is only active when light is either off
  /// or does not support any color modes and capabilities).
  UNKNOWN = 0,
  /// White output.
  WHITE = *ColorCapability::WHITE,
  /// White output with controllable color temperature.
  COLOR_TEMPERATURE = *ColorCapability::WHITE | *ColorCapability::COLOR_TEMPERATURE,
  /// Cold and warm white output with controllable intensities.
  COLD_WARM_WHITE = *ColorCapability::COLD_WARM_WHITE,
  /// RGB color output.
  RGB = *ColorCapability::RGB,
  /// RGB color output and a white output.
  RGB_WHITE = *ColorCapability::RGB | *ColorCapability::WHITE,
  /// RGB color output and a white output with controllable color temperature.
  RGB_COLOR_TEMPERATURE = *ColorCapability::RGB | *ColorCapability::WHITE | *ColorCapability::COLOR_TEMPERATURE,
  /// RGB color output, and cold and warm white outputs.
  RGB_COLD_WARM_WHITE = *ColorCapability::RGB | *ColorCapability::COLD_WARM_WHITE
};

// Define unary * operator to convert enum to underlying value -- this saves a lot of typing and ugly code.
inline constexpr uint8_t operator*(ColorMode val) { return static_cast<uint8_t>(val); }

}  // namespace light
}  // namespace esphome
