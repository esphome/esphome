# EnumBitmask Pattern Documentation

## Overview

`EnumBitmask<EnumType, MaxBits>` from `esphome/core/enum_bitmask.h` provides a memory-efficient replacement for `std::set<EnumType>` when storing sets of enum values.

## When to Use

Use `EnumBitmask` instead of `std::set<EnumType>` when:
- Storing sets of enum values (e.g., supported modes, capabilities)
- Enum has ≤32 distinct values
- Memory efficiency is important (saves ~586 bytes per `std::set` instance)

## Benefits

- **Memory Savings**: Eliminates red-black tree overhead (~586 bytes per instance)
- **Compact Storage**: 1-4 bytes depending on enum count (uint8_t/uint16_t/uint32_t)
- **Constexpr-Compatible**: Supports compile-time initialization
- **Efficient Iteration**: Only visits set bits, not all possible enum values
- **Range-Based Loops**: `for (auto value : mask)` works seamlessly

## Requirements

1. Enum must have sequential values (or use a lookup table for mapping)
2. Maximum 32 enum values (uint32_t bitmask limitation)
3. Must provide template specializations for `enum_to_bit()` and `bit_to_enum()`

## Basic Usage Example

```cpp
// Bad - red-black tree overhead (~586 bytes)
std::set<ColorMode> supported_modes;
supported_modes.insert(ColorMode::RGB);
supported_modes.insert(ColorMode::WHITE);
if (supported_modes.count(ColorMode::RGB)) { ... }

// Good - compact bitmask storage (2-4 bytes)
ColorModeMask supported_modes({ColorMode::RGB, ColorMode::WHITE});
if (supported_modes.contains(ColorMode::RGB)) { ... }
for (auto mode : supported_modes) { ... }  // Iterate over set values
```

## Implementation Pattern

### 1. Define the Lookup Table

If enum values aren't sequential from 0, create a lookup table:

```cpp
// In your component header (e.g., esphome/components/light/color_mode.h)
constexpr ColorMode COLOR_MODE_LOOKUP[10] = {
    ColorMode::UNKNOWN,      // bit 0
    ColorMode::ON_OFF,       // bit 1
    ColorMode::BRIGHTNESS,   // bit 2
    ColorMode::WHITE,        // bit 3
    ColorMode::COLOR_TEMPERATURE,      // bit 4
    ColorMode::COLD_WARM_WHITE,        // bit 5
    ColorMode::RGB,                    // bit 6
    ColorMode::RGB_WHITE,              // bit 7
    ColorMode::RGB_COLOR_TEMPERATURE,  // bit 8
    ColorMode::RGB_COLD_WARM_WHITE,    // bit 9
};
```

### 2. Create Type Alias

```cpp
constexpr int COLOR_MODE_BITMASK_SIZE = 10;
using ColorModeMask = EnumBitmask<ColorMode, COLOR_MODE_BITMASK_SIZE>;
```

### 3. Provide Template Specializations

**IMPORTANT**: Specializations must be in the **global namespace** (C++ requirement). Place them at the end of your header file, outside your component namespace.

```cpp
// At end of header, outside namespace esphome::light
// Template specializations for ColorMode must be in global namespace
//
// C++ requires template specializations to be declared in the same namespace as the
// original template. Since EnumBitmask is in the esphome namespace (not esphome::light),
// we must provide these specializations at global scope with fully-qualified names.
//
// These specializations define how ColorMode enum values map to/from bit positions.

/// Map ColorMode enum values to bit positions (0-9)
template<>
constexpr int esphome::EnumBitmask<esphome::light::ColorMode,
                                   esphome::light::COLOR_MODE_BITMASK_SIZE>::enum_to_bit(
    esphome::light::ColorMode mode) {
  // Map enum value to bit position (0-9)
  for (int i = 0; i < esphome::light::COLOR_MODE_BITMASK_SIZE; ++i) {
    if (esphome::light::COLOR_MODE_LOOKUP[i] == mode)
      return i;
  }
  return 0;  // Unknown values map to bit 0 (typically reserved for UNKNOWN/NONE)
}

/// Map bit positions (0-9) to ColorMode enum values
template<>
inline esphome::light::ColorMode esphome::EnumBitmask<esphome::light::ColorMode,
                                                      esphome::light::COLOR_MODE_BITMASK_SIZE>::bit_to_enum(int bit) {
  return (bit >= 0 && bit < esphome::light::COLOR_MODE_BITMASK_SIZE)
         ? esphome::light::COLOR_MODE_LOOKUP[bit]
         : esphome::light::ColorMode::UNKNOWN;
}
```

### Error Handling in enum_to_bit()

The implementation returns bit 0 for unknown enum values:
```cpp
return 0;  // Unknown values map to bit 0
```

This means an unknown ColorMode maps to the same bit as `ColorMode::UNKNOWN`. This is acceptable because:
- Compile-time failure occurs if using invalid enum values
- `ColorMode::UNKNOWN` at bit 0 is semantically correct
- Runtime misuse is prevented by type safety

## API Compatibility with std::set

EnumBitmask provides both modern `.contains()` / `.add()` / `.remove()` methods and std::set-compatible aliases for drop-in replacement:

| Operation | std::set | EnumBitmask | Notes |
|-----------|----------|-------------|-------|
| Add value | `.insert(value)` | `.insert(value)` or `.add(value)` | Both work |
| Check membership | `.count(value)` | `.count(value)` or `.contains(value)` | Both work |
| Remove value | `.erase(value)` | `.erase(value)` or `.remove(value)` | Both work |
| Count elements | `.size()` | `.size()` | Same |
| Check empty | `.empty()` | `.empty()` | Same |
| Clear all | `.clear()` | `.clear()` | Same |
| Iterate | `for (auto v : set)` | `for (auto v : mask)` | Same |

**Drop-in replacement**: You can use either the std::set-compatible methods (`.insert()`, `.count()`, `.erase()`) or the more explicit methods (`.add()`, `.contains()`, `.remove()`).

## Complete Usage Example

See `esphome/components/light/color_mode.h` for a complete real-world implementation showing:
- Lookup table definition
- Type aliases
- Template specializations
- Helper functions using the bitmask

## Common Patterns

### Compile-Time Initialization

```cpp
// Constexpr-compatible for compile-time initialization
constexpr ColorModeMask DEFAULT_MODES({ColorMode::ON_OFF, ColorMode::BRIGHTNESS});
```

### Adding Multiple Values

```cpp
ColorModeMask modes;
modes.add({ColorMode::RGB, ColorMode::WHITE, ColorMode::COLOR_TEMPERATURE});
```

### Checking and Iterating

```cpp
if (modes.contains(ColorMode::RGB)) {
  // RGB mode is supported
}

for (auto mode : modes) {
  // Process each supported mode
  ESP_LOGD(TAG, "Supported mode: %d", static_cast<int>(mode));
}
```

### Working with Raw Bitmask Values

```cpp
// Get raw bitmask for bitwise operations
auto mask = modes.get_mask();

// Check if raw bitmask contains a value
if (ColorModeMask::mask_contains(mask, ColorMode::RGB)) { ... }

// Get first value from raw bitmask
auto first = ColorModeMask::first_value_from_mask(mask);
```

## Detection of Opportunities

Look for these patterns in existing code:
- `std::set<EnumType>` with small enum sets (≤32 values)
- Components storing "supported modes" or "capabilities"
- Red-black tree code (`rb_tree`, `_Rb_tree`) in compiler output
- Flash size increases when adding enum set storage

## When NOT to Use

- Enum has >32 distinct values (bitmask limitation)
- Need to store arbitrary runtime-determined integer values (not enum values)
- Enum values are sparse or non-sequential and lookup table would be impractical
- Code readability matters more than memory savings (niche single-use components)
