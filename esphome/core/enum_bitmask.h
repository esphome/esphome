#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <type_traits>

namespace esphome {

/// Generic bitmask for storing a set of enum values efficiently.
/// Replaces std::set<EnumType> to eliminate red-black tree overhead (~586 bytes per instantiation).
///
/// Template parameters:
///   EnumType: The enum type to store (must be uint8_t-based)
///   MaxBits: Maximum number of bits needed (auto-selects uint8_t/uint16_t/uint32_t)
///
/// Requirements:
///   - EnumType must be an enum with sequential values starting from 0
///   - Specialization must provide enum_to_bit() and bit_to_enum() static methods
///   - MaxBits must be sufficient to hold all enum values
///
/// Example usage:
///   using ClimateModeMask = EnumBitmask<ClimateMode, 8>;
///   ClimateModeMask modes({CLIMATE_MODE_HEAT, CLIMATE_MODE_COOL});
///   if (modes.count(CLIMATE_MODE_HEAT)) { ... }
///   for (auto mode : modes) { ... }  // Iterate over set bits
///
/// For complete usage examples with template specializations, see:
///   - esphome/components/light/color_mode.h (ColorMode example)
///
/// Design notes:
///   - Uses compile-time type selection for optimal size (uint8_t/uint16_t/uint32_t)
///   - Iterator converts bit positions to actual enum values during traversal
///   - All operations are constexpr-compatible for compile-time initialization
///   - Drop-in replacement for std::set<EnumType> with simpler API
///
template<typename EnumType, int MaxBits = 16> class EnumBitmask {
 public:
  // Automatic bitmask type selection based on MaxBits
  // ≤8 bits: uint8_t, ≤16 bits: uint16_t, otherwise: uint32_t
  using bitmask_t =
      typename std::conditional<(MaxBits <= 8), uint8_t,
                                typename std::conditional<(MaxBits <= 16), uint16_t, uint32_t>::type>::type;

  constexpr EnumBitmask() = default;

  /// Construct from initializer list: {VALUE1, VALUE2, ...}
  constexpr EnumBitmask(std::initializer_list<EnumType> values) {
    for (auto value : values) {
      this->insert(value);
    }
  }

  /// Add a single enum value to the set (std::set compatibility)
  constexpr void insert(EnumType value) { this->mask_ |= (static_cast<bitmask_t>(1) << enum_to_bit(value)); }

  /// Add multiple enum values from initializer list
  constexpr void insert(std::initializer_list<EnumType> values) {
    for (auto value : values) {
      this->insert(value);
    }
  }

  /// Remove an enum value from the set (std::set compatibility)
  constexpr void erase(EnumType value) { this->mask_ &= ~(static_cast<bitmask_t>(1) << enum_to_bit(value)); }

  /// Clear all values from the set
  constexpr void clear() { this->mask_ = 0; }

  /// Check if the set contains a specific enum value (std::set compatibility)
  /// Returns 1 if present, 0 if not (same as std::set for unique elements)
  constexpr size_t count(EnumType value) const {
    return (this->mask_ & (static_cast<bitmask_t>(1) << enum_to_bit(value))) != 0 ? 1 : 0;
  }

  /// Count the number of enum values in the set
  constexpr size_t size() const {
    // Brian Kernighan's algorithm - efficient for sparse bitmasks
    // Typical case: 2-4 modes out of 10 possible
    bitmask_t n = this->mask_;
    size_t count = 0;
    while (n) {
      n &= n - 1;  // Clear the least significant set bit
      count++;
    }
    return count;
  }

  /// Check if the set is empty
  constexpr bool empty() const { return this->mask_ == 0; }

  /// Iterator support for range-based for loops and API encoding
  /// Iterates over set bits and converts bit positions to enum values
  class Iterator {
   public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = EnumType;
    using difference_type = std::ptrdiff_t;
    using pointer = const EnumType *;
    using reference = EnumType;

    constexpr Iterator(bitmask_t mask, int bit) : mask_(mask), bit_(bit) { advance_to_next_set_bit_(); }

    constexpr EnumType operator*() const { return bit_to_enum(bit_); }

    constexpr Iterator &operator++() {
      ++bit_;
      advance_to_next_set_bit_();
      return *this;
    }

    constexpr bool operator==(const Iterator &other) const { return bit_ == other.bit_; }

    constexpr bool operator!=(const Iterator &other) const { return !(*this == other); }

   private:
    constexpr void advance_to_next_set_bit_() { bit_ = find_next_set_bit(mask_, bit_); }

    bitmask_t mask_;
    int bit_;
  };

  constexpr Iterator begin() const { return Iterator(mask_, 0); }
  constexpr Iterator end() const { return Iterator(mask_, MaxBits); }

  /// Get the raw bitmask value for optimized operations
  constexpr bitmask_t get_mask() const { return this->mask_; }

  /// Check if a specific enum value is present in a raw bitmask
  /// Useful for checking intersection results without creating temporary objects
  static constexpr bool mask_contains(bitmask_t mask, EnumType value) {
    return (mask & (static_cast<bitmask_t>(1) << enum_to_bit(value))) != 0;
  }

  /// Get the first enum value from a raw bitmask
  /// Used for optimizing intersection logic (e.g., "pick first suitable mode")
  static constexpr EnumType first_value_from_mask(bitmask_t mask) { return bit_to_enum(find_next_set_bit(mask, 0)); }

  /// Find the next set bit in a bitmask starting from a given position
  /// Returns the bit position, or MaxBits if no more bits are set
  static constexpr int find_next_set_bit(bitmask_t mask, int start_bit) {
    int bit = start_bit;
    while (bit < MaxBits && !(mask & (static_cast<bitmask_t>(1) << bit))) {
      ++bit;
    }
    return bit;
  }

 protected:
  // Must be provided by template specialization
  // These convert between enum values and bit positions (0, 1, 2, ...)
  static constexpr int enum_to_bit(EnumType value);
  static EnumType bit_to_enum(int bit);  // Not constexpr due to static array limitation in C++20

  bitmask_t mask_{0};
};

}  // namespace esphome
