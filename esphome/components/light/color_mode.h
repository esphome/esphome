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

// Constants for ColorMode count and bit range
static constexpr int COLOR_MODE_COUNT = 10;                             // UNKNOWN through RGB_COLD_WARM_WHITE
static constexpr int MAX_BIT_INDEX = sizeof(color_mode_bitmask_t) * 8;  // Number of bits in bitmask type

// Compile-time array of all ColorMode values in declaration order
// Bit positions (0-9) map directly to enum declaration order
static constexpr ColorMode COLOR_MODES[COLOR_MODE_COUNT] = {
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

/// Map ColorMode enum values to bit positions (0-9)
/// Bit positions follow the enum declaration order
static constexpr int mode_to_bit(ColorMode mode) {
  // Linear search through COLOR_MODES array
  // Compiler optimizes this to efficient code since array is constexpr
  for (int i = 0; i < COLOR_MODE_COUNT; ++i) {
    if (COLOR_MODES[i] == mode)
      return i;
  }
  return 0;
}

/// Map bit positions (0-9) to ColorMode enum values
/// Bit positions follow the enum declaration order
static constexpr ColorMode bit_to_mode(int bit) {
  // Direct lookup in COLOR_MODES array
  return (bit >= 0 && bit < COLOR_MODE_COUNT) ? COLOR_MODES[bit] : ColorMode::UNKNOWN;
}

/// Helper to compute capability bitmask at compile time
static constexpr color_mode_bitmask_t compute_capability_bitmask(ColorCapability capability) {
  color_mode_bitmask_t mask = 0;
  uint8_t cap_bit = static_cast<uint8_t>(capability);

  // Check each ColorMode to see if it has this capability
  for (int bit = 0; bit < COLOR_MODE_COUNT; ++bit) {
    uint8_t mode_val = static_cast<uint8_t>(bit_to_mode(bit));
    if ((mode_val & cap_bit) != 0) {
      mask |= (1 << bit);
    }
  }
  return mask;
}

// Number of ColorCapability enum values
static constexpr int COLOR_CAPABILITY_COUNT = 6;

/// Compile-time lookup table mapping ColorCapability to bitmask
/// This array is computed at compile time using constexpr
static constexpr color_mode_bitmask_t CAPABILITY_BITMASKS[] = {
    compute_capability_bitmask(ColorCapability::ON_OFF),             // 1 << 0
    compute_capability_bitmask(ColorCapability::BRIGHTNESS),         // 1 << 1
    compute_capability_bitmask(ColorCapability::WHITE),              // 1 << 2
    compute_capability_bitmask(ColorCapability::COLOR_TEMPERATURE),  // 1 << 3
    compute_capability_bitmask(ColorCapability::COLD_WARM_WHITE),    // 1 << 4
    compute_capability_bitmask(ColorCapability::RGB),                // 1 << 5
};

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

  /// Add multiple modes at once using initializer list
  constexpr void add(std::initializer_list<ColorMode> modes) {
    for (auto mode : modes) {
      this->add(mode);
    }
  }

  constexpr bool contains(ColorMode mode) const { return (this->mask_ & (1 << mode_to_bit(mode))) != 0; }

  constexpr size_t size() const {
    // Count set bits using Brian Kernighan's algorithm
    // More efficient for sparse bitmasks (typical case: 2-4 modes out of 10)
    uint16_t n = this->mask_;
    size_t count = 0;
    while (n) {
      n &= n - 1;  // Clear the least significant set bit
      count++;
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
    constexpr void advance_to_next_set_bit_() { bit_ = ColorModeMask::find_next_set_bit(mask_, bit_); }

    color_mode_bitmask_t mask_;
    int bit_;
  };

  constexpr Iterator begin() const { return Iterator(mask_, 0); }
  constexpr Iterator end() const { return Iterator(mask_, MAX_BIT_INDEX); }

  /// Get the raw bitmask value for API encoding
  constexpr color_mode_bitmask_t get_mask() const { return this->mask_; }

  /// Find the next set bit in a bitmask starting from a given position
  /// Returns the bit position, or MAX_BIT_INDEX if no more bits are set
  static constexpr int find_next_set_bit(color_mode_bitmask_t mask, int start_bit) {
    int bit = start_bit;
    while (bit < MAX_BIT_INDEX && !(mask & (1 << bit))) {
      ++bit;
    }
    return bit;
  }

  /// Find the first set bit in a bitmask and return the corresponding ColorMode
  /// Used for optimizing compute_color_mode_() intersection logic
  static constexpr ColorMode first_mode_from_mask(color_mode_bitmask_t mask) {
    return bit_to_mode(find_next_set_bit(mask, 0));
  }

  /// Check if a ColorMode is present in a raw bitmask value
  /// Useful for checking intersection results without creating a temporary ColorModeMask
  static constexpr bool mask_contains(color_mode_bitmask_t mask, ColorMode mode) {
    return (mask & (1 << mode_to_bit(mode))) != 0;
  }

  /// Check if any mode in the bitmask has a specific capability
  /// Used for checking if a light supports a capability (e.g., BRIGHTNESS, RGB)
  bool has_capability(ColorCapability capability) const {
    // Lookup the pre-computed bitmask for this capability and check intersection with our mask
    // ColorCapability values: 1, 2, 4, 8, 16, 32 -> array indices: 0, 1, 2, 3, 4, 5
    // We need to convert the power-of-2 value to an index
    uint8_t cap_val = static_cast<uint8_t>(capability);
#if defined(__GNUC__) || defined(__clang__)
    // Use compiler intrinsic for efficient bit position lookup (O(1) vs O(log n))
    int index = __builtin_ctz(cap_val);
#else
    // Fallback for compilers without __builtin_ctz
    int index = 0;
    while (cap_val > 1) {
      cap_val >>= 1;
      ++index;
    }
#endif
    return (this->mask_ & CAPABILITY_BITMASKS[index]) != 0;
  }

 private:
  // Using uint16_t instead of uint32_t for more efficient iteration (fewer bits to scan).
  // Currently only 10 ColorMode values exist, so 16 bits is sufficient.
  // Can be changed to uint32_t if more than 16 color modes are needed in the future.
  // Note: Due to struct padding, uint16_t and uint32_t result in same LightTraits size (12 bytes).
  color_mode_bitmask_t mask_{0};
};

}  // namespace light
}  // namespace esphome
