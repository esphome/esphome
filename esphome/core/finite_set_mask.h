#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <type_traits>

namespace esphome {

/// Default bit mapping policy for contiguous enums starting at 0
/// Provides 1:1 mapping where enum value equals bit position
template<typename ValueType, int MaxBits> struct DefaultBitPolicy {
  // Automatic bitmask type selection based on MaxBits
  // ≤8 bits: uint8_t, ≤16 bits: uint16_t, otherwise: uint32_t
  using mask_t = typename std::conditional<(MaxBits <= 8), uint8_t,
                                           typename std::conditional<(MaxBits <= 16), uint16_t, uint32_t>::type>::type;

  static constexpr int MAX_BITS = MaxBits;

  static constexpr unsigned to_bit(ValueType value) { return static_cast<unsigned>(value); }

  static constexpr ValueType from_bit(unsigned bit) { return static_cast<ValueType>(bit); }
};

/// Generic bitmask for storing a finite set of discrete values efficiently.
/// Replaces std::set<ValueType> to eliminate red-black tree overhead (~586 bytes per instantiation).
///
/// Template parameters:
///   ValueType: The type to store (typically enum, but can be any discrete bounded type)
///   BitPolicy: Policy class defining bit mapping and mask type (defaults to DefaultBitPolicy)
///
/// BitPolicy requirements:
///   - using mask_t = <uint8_t|uint16_t|uint32_t>  // Bitmask storage type
///   - static constexpr int MAX_BITS               // Maximum number of bits
///   - static constexpr unsigned to_bit(ValueType) // Convert value to bit position
///   - static constexpr ValueType from_bit(unsigned) // Convert bit position to value
///
/// Example usage (1:1 mapping - climate enums):
///   // For contiguous enums starting at 0, use DefaultBitPolicy
///   using ClimateModeMask = FiniteSetMask<ClimateMode, DefaultBitPolicy<ClimateMode, CLIMATE_MODE_AUTO + 1>>;
///   ClimateModeMask modes({CLIMATE_MODE_HEAT, CLIMATE_MODE_COOL});
///   if (modes.count(CLIMATE_MODE_HEAT)) { ... }
///   for (auto mode : modes) { ... }
///
/// Example usage (custom mapping - ColorMode):
///   // For custom mappings, define a custom BitPolicy
///   // See esphome/components/light/color_mode.h for complete example
///
/// Design notes:
///   - Policy-based design allows custom bit mappings without template specialization
///   - Iterator converts bit positions to actual values during traversal
///   - All operations are constexpr-compatible for compile-time initialization
///   - Drop-in replacement for std::set<ValueType> with simpler API
///
template<typename ValueType, typename BitPolicy = DefaultBitPolicy<ValueType, 16>> class FiniteSetMask {
 public:
  using bitmask_t = typename BitPolicy::mask_t;

  constexpr FiniteSetMask() = default;

  /// Construct from initializer list: {VALUE1, VALUE2, ...}
  constexpr FiniteSetMask(std::initializer_list<ValueType> values) {
    for (auto value : values) {
      this->insert(value);
    }
  }

  /// Add a single value to the set (std::set compatibility)
  constexpr void insert(ValueType value) { this->mask_ |= (static_cast<bitmask_t>(1) << BitPolicy::to_bit(value)); }

  /// Add multiple values from initializer list
  constexpr void insert(std::initializer_list<ValueType> values) {
    for (auto value : values) {
      this->insert(value);
    }
  }

  /// Remove a value from the set (std::set compatibility)
  constexpr void erase(ValueType value) { this->mask_ &= ~(static_cast<bitmask_t>(1) << BitPolicy::to_bit(value)); }

  /// Clear all values from the set
  constexpr void clear() { this->mask_ = 0; }

  /// Check if the set contains a specific value (std::set compatibility)
  /// Returns 1 if present, 0 if not (same as std::set for unique elements)
  constexpr size_t count(ValueType value) const {
    return (this->mask_ & (static_cast<bitmask_t>(1) << BitPolicy::to_bit(value))) != 0 ? 1 : 0;
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
      return BitPolicy::from_bit(find_next_set_bit(mask_, 0));
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
    return (mask & (static_cast<bitmask_t>(1) << BitPolicy::to_bit(value))) != 0;
  }

  /// Get the first value from a raw bitmask
  /// Used for optimizing intersection logic (e.g., "pick first suitable mode")
  static constexpr ValueType first_value_from_mask(bitmask_t mask) {
    return BitPolicy::from_bit(find_next_set_bit(mask, 0));
  }

  /// Find the next set bit in a bitmask starting from a given position
  /// Returns the bit position, or MAX_BITS if no more bits are set
  static constexpr int find_next_set_bit(bitmask_t mask, int start_bit) {
    int bit = start_bit;
    while (bit < BitPolicy::MAX_BITS && !(mask & (static_cast<bitmask_t>(1) << bit))) {
      ++bit;
    }
    return bit;
  }

 protected:
  bitmask_t mask_{0};
};

}  // namespace esphome
