#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <type_traits>

namespace esphome {

/// Generic bitmask for storing a finite set of discrete values efficiently.
/// Replaces std::set<ValueType> to eliminate red-black tree overhead (~586 bytes per instantiation).
///
/// Template parameters:
///   ValueType: The type to store (typically enum, but can be any discrete bounded type)
///   MaxBits: Maximum number of bits needed (auto-selects uint8_t/uint16_t/uint32_t)
///
/// Requirements:
///   - ValueType must have a bounded discrete range that maps to bit positions
///   - For 1:1 mappings (contiguous enums starting at 0), no specialization needed
///   - For custom mappings (like ColorMode), specialize value_to_bit() and/or bit_to_value()
///   - MaxBits must be sufficient to hold all possible values
///
/// Example usage (1:1 mapping - climate enums):
///   // For enums with contiguous values starting at 0, no specialization needed!
///   using ClimateModeMask = FiniteSetMask<ClimateMode, 8>;
///   ClimateModeMask modes({CLIMATE_MODE_HEAT, CLIMATE_MODE_COOL});
///   if (modes.count(CLIMATE_MODE_HEAT)) { ... }
///   for (auto mode : modes) { ... }  // Iterate over set bits
///
/// Example usage (custom mapping - ColorMode):
///   // For non-contiguous enums or custom mappings, specialize value_to_bit() and/or bit_to_value()
///   // See esphome/components/light/color_mode.h for complete example
///
/// Design notes:
///   - Uses compile-time type selection for optimal size (uint8_t/uint16_t/uint32_t)
///   - Iterator converts bit positions to actual values during traversal
///   - All operations are constexpr-compatible for compile-time initialization
///   - Drop-in replacement for std::set<ValueType> with simpler API
///   - Despite the name, works with any discrete bounded type, not just enums
///
template<typename ValueType, int MaxBits = 16> class FiniteSetMask {
 public:
  // Automatic bitmask type selection based on MaxBits
  // ≤8 bits: uint8_t, ≤16 bits: uint16_t, otherwise: uint32_t
  using bitmask_t =
      typename std::conditional<(MaxBits <= 8), uint8_t,
                                typename std::conditional<(MaxBits <= 16), uint16_t, uint32_t>::type>::type;

  constexpr FiniteSetMask() = default;

  /// Construct from initializer list: {VALUE1, VALUE2, ...}
  constexpr FiniteSetMask(std::initializer_list<ValueType> values) {
    for (auto value : values) {
      this->insert(value);
    }
  }

  /// Add a single value to the set (std::set compatibility)
  constexpr void insert(ValueType value) { this->mask_ |= (static_cast<bitmask_t>(1) << value_to_bit(value)); }

  /// Add multiple values from initializer list
  constexpr void insert(std::initializer_list<ValueType> values) {
    for (auto value : values) {
      this->insert(value);
    }
  }

  /// Remove a value from the set (std::set compatibility)
  constexpr void erase(ValueType value) { this->mask_ &= ~(static_cast<bitmask_t>(1) << value_to_bit(value)); }

  /// Clear all values from the set
  constexpr void clear() { this->mask_ = 0; }

  /// Check if the set contains a specific value (std::set compatibility)
  /// Returns 1 if present, 0 if not (same as std::set for unique elements)
  constexpr size_t count(ValueType value) const {
    return (this->mask_ & (static_cast<bitmask_t>(1) << value_to_bit(value))) != 0 ? 1 : 0;
  }

  /// Count the number of values in the set
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
  /// Iterates over set bits and converts bit positions to values
  /// Optimization: removes bits from mask as we iterate
  class Iterator {
   public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = ValueType;
    using difference_type = std::ptrdiff_t;
    using pointer = const ValueType *;
    using reference = ValueType;

    constexpr explicit Iterator(bitmask_t mask) : mask_(mask) {}

    constexpr ValueType operator*() const {
      // Return value for the first set bit
      return bit_to_value(find_next_set_bit(mask_, 0));
    }

    constexpr Iterator &operator++() {
      // Clear the lowest set bit (Brian Kernighan's algorithm)
      mask_ &= mask_ - 1;
      return *this;
    }

    constexpr bool operator==(const Iterator &other) const { return mask_ == other.mask_; }

    constexpr bool operator!=(const Iterator &other) const { return !(*this == other); }

   private:
    bitmask_t mask_;
  };

  constexpr Iterator begin() const { return Iterator(mask_); }
  constexpr Iterator end() const { return Iterator(0); }

  /// Get the raw bitmask value for optimized operations
  constexpr bitmask_t get_mask() const { return this->mask_; }

  /// Check if a specific value is present in a raw bitmask
  /// Useful for checking intersection results without creating temporary objects
  static constexpr bool mask_contains(bitmask_t mask, ValueType value) {
    return (mask & (static_cast<bitmask_t>(1) << value_to_bit(value))) != 0;
  }

  /// Get the first value from a raw bitmask
  /// Used for optimizing intersection logic (e.g., "pick first suitable mode")
  static constexpr ValueType first_value_from_mask(bitmask_t mask) { return bit_to_value(find_next_set_bit(mask, 0)); }

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
  // Default implementations for 1:1 mapping (enum value = bit position)
  // For enums with contiguous values starting at 0, these defaults work as-is.
  // If you need custom mapping (like ColorMode), provide specializations.
  static constexpr int value_to_bit(ValueType value) { return static_cast<int>(value); }
  static constexpr ValueType bit_to_value(int bit) { return static_cast<ValueType>(bit); }

  bitmask_t mask_{0};
};

}  // namespace esphome
