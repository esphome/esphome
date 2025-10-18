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

/// Bitmask for storing a set of ColorMode values efficiently.
/// Replaces std::set<ColorMode> to eliminate red-black tree overhead (~586 bytes).
class ColorModeMask {
 public:
  constexpr ColorModeMask() = default;

  /// Support initializer list syntax: {ColorMode::RGB, ColorMode::WHITE}
  constexpr ColorModeMask(std::initializer_list<ColorMode> modes) {
    for (auto mode : modes) {
      this->add(mode);
    }
  }

  constexpr void add(ColorMode mode) { this->mask_ |= (1 << mode_to_bit(mode)); }

  constexpr bool contains(ColorMode mode) const { return (this->mask_ & (1 << mode_to_bit(mode))) != 0; }

  constexpr size_t size() const {
    // Count set bits
    uint16_t n = this->mask_;
    size_t count = 0;
    while (n) {
      count += n & 1;
      n >>= 1;
    }
    return count;
  }

  constexpr bool empty() const { return this->mask_ == 0; }

  /// Iterator support for API encoding
  class Iterator {
   public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = ColorMode;
    using difference_type = std::ptrdiff_t;
    using pointer = const ColorMode *;
    using reference = ColorMode;

    constexpr Iterator(color_mode_bitmask_t mask, int bit) : mask_(mask), bit_(bit) { advance_to_next_set_bit_(); }

    constexpr ColorMode operator*() const { return bit_to_mode(bit_); }

    constexpr Iterator &operator++() {
      ++bit_;
      advance_to_next_set_bit_();
      return *this;
    }

    constexpr bool operator==(const Iterator &other) const { return bit_ == other.bit_; }

    constexpr bool operator!=(const Iterator &other) const { return !(*this == other); }

   private:
    constexpr void advance_to_next_set_bit_() {
      while (bit_ < MAX_BIT_INDEX && !(mask_ & (1 << bit_))) {
        ++bit_;
      }
    }

    color_mode_bitmask_t mask_;
    int bit_;
  };

  constexpr Iterator begin() const { return Iterator(mask_, 0); }
  constexpr Iterator end() const { return Iterator(mask_, MAX_BIT_INDEX); }

  /// Get the raw bitmask value for API encoding
  constexpr color_mode_bitmask_t get_mask() const { return this->mask_; }

  /// Find the first set bit in a bitmask and return the corresponding ColorMode
  /// Used for optimizing compute_color_mode_() intersection logic
  static constexpr ColorMode first_mode_from_mask(color_mode_bitmask_t mask) {
    // Find the position of the first set bit (least significant bit)
    int bit = 0;
    while (bit < MAX_BIT_INDEX && !(mask & (1 << bit))) {
      ++bit;
    }
    return bit_to_mode(bit);
  }

  /// Check if a ColorMode is present in a raw bitmask value
  /// Useful for checking intersection results without creating a temporary ColorModeMask
  static constexpr bool mask_contains(color_mode_bitmask_t mask, ColorMode mode) {
    return (mask & (1 << mode_to_bit(mode))) != 0;
  }

  /// Check if any mode in the bitmask has a specific capability
  /// Used for checking if a light supports a capability (e.g., BRIGHTNESS, RGB)
  bool has_capability(ColorCapability capability) const {
    uint8_t cap_mask = static_cast<uint8_t>(capability);
    // Iterate through each mode and check if it has the capability
    for (auto mode : *this) {
      if (static_cast<uint8_t>(mode) & cap_mask) {
        return true;
      }
    }
    return false;
  }

  /// Build a bitmask of modes that match the given capability requirements
  /// @param require_caps Capabilities that must be present in the mode
  /// @param exclude_caps Capabilities that must not be present in the mode (for none case)
  /// @return Raw bitmask value
  static constexpr color_mode_bitmask_t build_mask_matching(uint8_t require_caps, uint8_t exclude_caps = 0) {
    color_mode_bitmask_t mask = 0;
    // Check each mode to see if it matches the requirements
    // Skip UNKNOWN (bit 0), iterate through actual color modes (bits 1-9)
    for (int bit = 1; bit < COLOR_MODE_COUNT; ++bit) {
      ColorMode mode = bit_to_mode(bit);
      uint8_t mode_val = static_cast<uint8_t>(mode);
      // Mode matches if it has all required caps and none of the excluded caps
      if ((mode_val & require_caps) == require_caps && (mode_val & exclude_caps) == 0) {
        mask |= (1 << bit);
      }
    }
    return mask;
  }

 private:
  // Using uint16_t instead of uint32_t for more efficient iteration (fewer bits to scan).
  // Currently only 10 ColorMode values exist, so 16 bits is sufficient.
  // Can be changed to uint32_t if more than 16 color modes are needed in the future.
  // Note: Due to struct padding, uint16_t and uint32_t result in same LightTraits size (12 bytes).
  color_mode_bitmask_t mask_{0};

  // Constants for ColorMode count and bit range
  static constexpr int COLOR_MODE_COUNT = 10;                             // UNKNOWN through RGB_COLD_WARM_WHITE
  static constexpr int MAX_BIT_INDEX = sizeof(color_mode_bitmask_t) * 8;  // Number of bits in bitmask type

  /// Map ColorMode enum values to bit positions (0-9)
  static constexpr int mode_to_bit(ColorMode mode) {
    // Using switch instead of lookup table to avoid RAM usage on ESP8266
    // The compiler optimizes this efficiently
    switch (mode) {
      case ColorMode::UNKNOWN:  // 0
        return 0;
      case ColorMode::ON_OFF:  // 1
        return 1;
      case ColorMode::BRIGHTNESS:  // 3
        return 2;
      case ColorMode::WHITE:  // 7
        return 3;
      case ColorMode::COLOR_TEMPERATURE:  // 11
        return 4;
      case ColorMode::COLD_WARM_WHITE:  // 19
        return 5;
      case ColorMode::RGB:  // 35
        return 6;
      case ColorMode::RGB_WHITE:  // 39
        return 7;
      case ColorMode::RGB_COLOR_TEMPERATURE:  // 47
        return 8;
      case ColorMode::RGB_COLD_WARM_WHITE:  // 51
        return 9;
      default:
        return 0;
    }
  }

  static constexpr ColorMode bit_to_mode(int bit) {
    // Using switch instead of lookup table to avoid RAM usage on ESP8266
    switch (bit) {
      case 0:
        return ColorMode::UNKNOWN;  // 0
      case 1:
        return ColorMode::ON_OFF;  // 1
      case 2:
        return ColorMode::BRIGHTNESS;  // 3
      case 3:
        return ColorMode::WHITE;  // 7
      case 4:
        return ColorMode::COLOR_TEMPERATURE;  // 11
      case 5:
        return ColorMode::COLD_WARM_WHITE;  // 19
      case 6:
        return ColorMode::RGB;  // 35
      case 7:
        return ColorMode::RGB_WHITE;  // 39
      case 8:
        return ColorMode::RGB_COLOR_TEMPERATURE;  // 47
      case 9:
        return ColorMode::RGB_COLD_WARM_WHITE;  // 51
      default:
        return ColorMode::UNKNOWN;
    }
  }
};

}  // namespace light
}  // namespace esphome
