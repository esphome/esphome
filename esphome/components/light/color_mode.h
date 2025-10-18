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

  /// Iterator support for API encoding
  class Iterator {
   public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = ColorMode;
    using difference_type = std::ptrdiff_t;
    using pointer = const ColorMode *;
    using reference = ColorMode;

    constexpr Iterator(uint16_t mask, int bit) : mask_(mask), bit_(bit) { advance_to_next_set_bit(); }

    constexpr ColorMode operator*() const { return bit_to_mode(bit_); }

    constexpr Iterator &operator++() {
      ++bit_;
      advance_to_next_set_bit();
      return *this;
    }

    constexpr bool operator==(const Iterator &other) const { return bit_ == other.bit_; }

    constexpr bool operator!=(const Iterator &other) const { return !(*this == other); }

   private:
    constexpr void advance_to_next_set_bit() {
      while (bit_ < 16 && !(mask_ & (1 << bit_))) {
        ++bit_;
      }
    }

    uint16_t mask_;
    int bit_;
  };

  constexpr Iterator begin() const { return Iterator(mask_, 0); }
  constexpr Iterator end() const { return Iterator(mask_, 16); }

  /// Get the raw bitmask value for API encoding
  constexpr uint16_t get_mask() const { return this->mask_; }

 private:
  // Using uint16_t instead of uint32_t for more efficient iteration (fewer bits to scan).
  // Currently only 10 ColorMode values exist, so 16 bits is sufficient.
  // Can be changed to uint32_t if more than 16 color modes are needed in the future.
  // Note: Due to struct padding, uint16_t and uint32_t result in same LightTraits size (12 bytes).
  uint16_t mask_{0};

  /// Map ColorMode enum values to bit positions (0-9)
  static constexpr int mode_to_bit(ColorMode mode) {
    // Using switch instead of lookup table to avoid RAM usage on ESP8266
    // The compiler optimizes this efficiently
    switch (mode) {
      case ColorMode::UNKNOWN:
        return 0;
      case ColorMode::ON_OFF:
        return 1;
      case ColorMode::BRIGHTNESS:
        return 2;
      case ColorMode::WHITE:
        return 3;
      case ColorMode::COLOR_TEMPERATURE:
        return 4;
      case ColorMode::COLD_WARM_WHITE:
        return 5;
      case ColorMode::RGB:
        return 6;
      case ColorMode::RGB_WHITE:
        return 7;
      case ColorMode::RGB_COLOR_TEMPERATURE:
        return 8;
      case ColorMode::RGB_COLD_WARM_WHITE:
        return 9;
      default:
        return 0;
    }
  }

  static constexpr ColorMode bit_to_mode(int bit) {
    // Using switch instead of lookup table to avoid RAM usage on ESP8266
    switch (bit) {
      case 0:
        return ColorMode::UNKNOWN;
      case 1:
        return ColorMode::ON_OFF;
      case 2:
        return ColorMode::BRIGHTNESS;
      case 3:
        return ColorMode::WHITE;
      case 4:
        return ColorMode::COLOR_TEMPERATURE;
      case 5:
        return ColorMode::COLD_WARM_WHITE;
      case 6:
        return ColorMode::RGB;
      case 7:
        return ColorMode::RGB_WHITE;
      case 8:
        return ColorMode::RGB_COLOR_TEMPERATURE;
      case 9:
        return ColorMode::RGB_COLD_WARM_WHITE;
      default:
        return ColorMode::UNKNOWN;
    }
  }
};

}  // namespace light
}  // namespace esphome
