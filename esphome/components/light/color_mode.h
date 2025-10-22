#pragma once

#include <cstdint>
#include "esphome/core/finite_set_mask.h"

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

/// Helper class to allow bitwise operations on ColorCapability
class ColorCapabilityHelper {
 public:
  constexpr ColorCapabilityHelper(ColorCapability val) : val_(val) {}
  constexpr operator ColorCapability() const { return val_; }
  constexpr operator uint8_t() const { return static_cast<uint8_t>(val_); }
  constexpr operator bool() const { return static_cast<uint8_t>(val_) != 0; }

 protected:
  ColorCapability val_;
};
constexpr ColorCapabilityHelper operator&(ColorCapability lhs, ColorCapability rhs) {
  return static_cast<ColorCapability>(static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs));
}
constexpr ColorCapabilityHelper operator&(ColorCapabilityHelper lhs, ColorCapability rhs) {
  return static_cast<ColorCapability>(static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs));
}
constexpr ColorCapabilityHelper operator|(ColorCapability lhs, ColorCapability rhs) {
  return static_cast<ColorCapability>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
}
constexpr ColorCapabilityHelper operator|(ColorCapabilityHelper lhs, ColorCapability rhs) {
  return static_cast<ColorCapability>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
}

/// Color modes are a combination of color capabilities that can be used at the same time.
enum class ColorMode : uint8_t {
  /// No color mode configured (cannot be a supported mode, only active when light is off).
  UNKNOWN = 0,
  /// Only on/off control.
  ON_OFF = (uint8_t) ColorCapability::ON_OFF,
  /// Dimmable light.
  BRIGHTNESS = (uint8_t) (ColorCapability::ON_OFF | ColorCapability::BRIGHTNESS),
  /// White output only (use only if the light also has another color mode such as RGB).
  WHITE = (uint8_t) (ColorCapability::ON_OFF | ColorCapability::BRIGHTNESS | ColorCapability::WHITE),
  /// Controllable color temperature output.
  COLOR_TEMPERATURE =
      (uint8_t) (ColorCapability::ON_OFF | ColorCapability::BRIGHTNESS | ColorCapability::COLOR_TEMPERATURE),
  /// Cold and warm white output with individually controllable brightness.
  COLD_WARM_WHITE =
      (uint8_t) (ColorCapability::ON_OFF | ColorCapability::BRIGHTNESS | ColorCapability::COLD_WARM_WHITE),
  /// RGB color output.
  RGB = (uint8_t) (ColorCapability::ON_OFF | ColorCapability::BRIGHTNESS | ColorCapability::RGB),
  /// RGB color output and a separate white output.
  RGB_WHITE =
      (uint8_t) (ColorCapability::ON_OFF | ColorCapability::BRIGHTNESS | ColorCapability::RGB | ColorCapability::WHITE),
  /// RGB color output and a separate white output with controllable color temperature.
  RGB_COLOR_TEMPERATURE = (uint8_t) (ColorCapability::ON_OFF | ColorCapability::BRIGHTNESS | ColorCapability::RGB |
                                     ColorCapability::WHITE | ColorCapability::COLOR_TEMPERATURE),
  /// RGB color output, and separate cold and warm white outputs.
  RGB_COLD_WARM_WHITE = (uint8_t) (ColorCapability::ON_OFF | ColorCapability::BRIGHTNESS | ColorCapability::RGB |
                                   ColorCapability::COLD_WARM_WHITE),
};

/// Helper class to allow bitwise operations on ColorMode with ColorCapability
class ColorModeHelper {
 public:
  constexpr ColorModeHelper(ColorMode val) : val_(val) {}
  constexpr operator ColorMode() const { return val_; }
  constexpr operator uint8_t() const { return static_cast<uint8_t>(val_); }
  constexpr operator bool() const { return static_cast<uint8_t>(val_) != 0; }

 protected:
  ColorMode val_;
};
constexpr ColorModeHelper operator&(ColorMode lhs, ColorMode rhs) {
  return static_cast<ColorMode>(static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs));
}
constexpr ColorModeHelper operator&(ColorMode lhs, ColorCapability rhs) {
  return static_cast<ColorMode>(static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs));
}
constexpr ColorModeHelper operator&(ColorModeHelper lhs, ColorMode rhs) {
  return static_cast<ColorMode>(static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs));
}
constexpr ColorModeHelper operator|(ColorMode lhs, ColorMode rhs) {
  return static_cast<ColorMode>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
}
constexpr ColorModeHelper operator|(ColorMode lhs, ColorCapability rhs) {
  return static_cast<ColorMode>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
}
constexpr ColorModeHelper operator|(ColorModeHelper lhs, ColorMode rhs) {
  return static_cast<ColorMode>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
}

// Type alias for raw color mode bitmask values
using color_mode_bitmask_t = uint16_t;

// Number of ColorMode enum values
constexpr int COLOR_MODE_BITMASK_SIZE = 10;

// Shared lookup table for ColorMode bit mapping
// This array defines the canonical order of color modes (bit 0-9)
// Declared early so it can be used by constexpr functions
constexpr ColorMode COLOR_MODE_LOOKUP[COLOR_MODE_BITMASK_SIZE] = {
    ColorMode::UNKNOWN,                // bit 0
    ColorMode::ON_OFF,                 // bit 1
    ColorMode::BRIGHTNESS,             // bit 2
    ColorMode::WHITE,                  // bit 3
    ColorMode::COLOR_TEMPERATURE,      // bit 4
    ColorMode::COLD_WARM_WHITE,        // bit 5
    ColorMode::RGB,                    // bit 6
    ColorMode::RGB_WHITE,              // bit 7
    ColorMode::RGB_COLOR_TEMPERATURE,  // bit 8
    ColorMode::RGB_COLD_WARM_WHITE,    // bit 9
};

// Type alias for ColorMode bitmask using generic FiniteSetMask template
using ColorModeMask = FiniteSetMask<ColorMode, COLOR_MODE_BITMASK_SIZE>;

// Number of ColorCapability enum values
constexpr int COLOR_CAPABILITY_COUNT = 6;

/// Helper to compute capability bitmask at compile time
constexpr uint16_t compute_capability_bitmask(ColorCapability capability) {
  uint16_t mask = 0;
  uint8_t cap_bit = static_cast<uint8_t>(capability);

  // Check each ColorMode to see if it has this capability
  for (int bit = 0; bit < COLOR_MODE_BITMASK_SIZE; ++bit) {
    uint8_t mode_val = static_cast<uint8_t>(COLOR_MODE_LOOKUP[bit]);
    if ((mode_val & cap_bit) != 0) {
      mask |= (1 << bit);
    }
  }
  return mask;
}

/// Compile-time lookup table mapping ColorCapability to bitmask
/// This array is computed at compile time using constexpr
constexpr uint16_t CAPABILITY_BITMASKS[] = {
    compute_capability_bitmask(ColorCapability::ON_OFF),             // 1 << 0
    compute_capability_bitmask(ColorCapability::BRIGHTNESS),         // 1 << 1
    compute_capability_bitmask(ColorCapability::WHITE),              // 1 << 2
    compute_capability_bitmask(ColorCapability::COLOR_TEMPERATURE),  // 1 << 3
    compute_capability_bitmask(ColorCapability::COLD_WARM_WHITE),    // 1 << 4
    compute_capability_bitmask(ColorCapability::RGB),                // 1 << 5
};

/// Convert a power-of-2 ColorCapability value to an array index
/// ColorCapability values: 1, 2, 4, 8, 16, 32 -> array indices: 0, 1, 2, 3, 4, 5
inline int capability_to_index(ColorCapability capability) {
  uint8_t cap_val = static_cast<uint8_t>(capability);
#if defined(__GNUC__) || defined(__clang__)
  // Use compiler intrinsic for efficient bit position lookup (O(1) vs O(log n))
  return __builtin_ctz(cap_val);
#else
  // Fallback for compilers without __builtin_ctz
  int index = 0;
  while (cap_val > 1) {
    cap_val >>= 1;
    ++index;
  }
  return index;
#endif
}

/// Check if any mode in the bitmask has a specific capability
/// Used for checking if a light supports a capability (e.g., BRIGHTNESS, RGB)
inline bool has_capability(const ColorModeMask &mask, ColorCapability capability) {
  // Lookup the pre-computed bitmask for this capability and check intersection with our mask
  return (mask.get_mask() & CAPABILITY_BITMASKS[capability_to_index(capability)]) != 0;
}

}  // namespace light
}  // namespace esphome

// Template specializations for ColorMode must be in global namespace
//
// C++ requires template specializations to be declared in the same namespace as the
// original template. Since FiniteSetMask is in the esphome namespace (not esphome::light),
// we must provide these specializations at global scope with fully-qualified names.
//
// These specializations define how ColorMode enum values map to/from bit positions.

/// Map ColorMode enum values to bit positions (0-9)
/// Bit positions follow the enum declaration order
template<>
constexpr int esphome::FiniteSetMask<esphome::light::ColorMode, esphome::light::COLOR_MODE_BITMASK_SIZE>::value_to_bit(
    esphome::light::ColorMode mode) {
  // Linear search through COLOR_MODE_LOOKUP array
  // Compiler optimizes this to efficient code since array is constexpr
  for (int i = 0; i < esphome::light::COLOR_MODE_BITMASK_SIZE; ++i) {
    if (esphome::light::COLOR_MODE_LOOKUP[i] == mode)
      return i;
  }
  return 0;
}

/// Map bit positions (0-9) to ColorMode enum values
/// Bit positions follow the enum declaration order
template<>
inline esphome::light::ColorMode esphome::FiniteSetMask<
    esphome::light::ColorMode, esphome::light::COLOR_MODE_BITMASK_SIZE>::bit_to_value(int bit) {
  return (bit >= 0 && bit < esphome::light::COLOR_MODE_BITMASK_SIZE) ? esphome::light::COLOR_MODE_LOOKUP[bit]
                                                                     : esphome::light::ColorMode::UNKNOWN;
}
