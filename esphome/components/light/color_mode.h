#pragma once

#include <cstdint>

namespace esphome {
namespace light {

/// Color channels are the various outputs that a light has and that can be independently controlled by the user.
enum class ColorChannel : uint8_t {
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
inline constexpr uint8_t operator*(ColorChannel val) { return static_cast<uint8_t>(val); }

/// Color modes are a combination of color channels that can be used at the same time.
enum class ColorMode : uint8_t {
  /// No color mode configured (cannot be a supported mode, is only active when light is either off
  /// or does not support any color modes and channels).
  UNKNOWN = 0,
  /// White output.
  WHITE = *ColorChannel::WHITE,
  /// White output with controllable color temperature.
  COLOR_TEMPERATURE = *ColorChannel::WHITE | *ColorChannel::COLOR_TEMPERATURE,
  /// Cold and warm white output with controllable intensities.
  COLD_WARM_WHITE = *ColorChannel::COLD_WARM_WHITE,
  /// RGB color output.
  RGB = *ColorChannel::RGB,
  /// RGB color output and a white output.
  RGB_WHITE = *ColorChannel::RGB | *ColorChannel::WHITE,
  /// RGB color output and a white output with controllable color temperature.
  RGB_COLOR_TEMPERATURE = *ColorChannel::RGB | *ColorChannel::WHITE | *ColorChannel::COLOR_TEMPERATURE,
  /// RGB color output, and cold and warm white outputs.
  RGB_COLD_WARM_WHITE = *ColorChannel::RGB | *ColorChannel::COLD_WARM_WHITE
};

// Define unary * operator to convert enum to underlying value -- this saves a lot of typing and ugly code.
inline constexpr uint8_t operator*(ColorMode val) { return static_cast<uint8_t>(val); }

}  // namespace light
}  // namespace esphome
