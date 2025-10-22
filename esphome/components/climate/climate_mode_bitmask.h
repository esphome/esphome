#pragma once

#include "esphome/core/enum_bitmask.h"
#include "climate_mode.h"

namespace esphome {
namespace climate {

// Type aliases for climate enum bitmasks
// These replace std::set<EnumType> to eliminate red-black tree overhead

// Bitmask size constants - sized to fit all enum values
constexpr int CLIMATE_MODE_BITMASK_SIZE = 8;  // 7 values (OFF, HEAT_COOL, COOL, HEAT, FAN_ONLY, DRY, AUTO)
constexpr int CLIMATE_FAN_MODE_BITMASK_SIZE =
    16;  // 10 values (ON, OFF, AUTO, LOW, MEDIUM, HIGH, MIDDLE, FOCUS, DIFFUSE, QUIET)
constexpr int CLIMATE_SWING_MODE_BITMASK_SIZE = 8;  // 4 values (OFF, BOTH, VERTICAL, HORIZONTAL)
constexpr int CLIMATE_PRESET_BITMASK_SIZE = 8;      // 8 values (NONE, HOME, AWAY, BOOST, COMFORT, ECO, SLEEP, ACTIVITY)

using ClimateModeMask = EnumBitmask<ClimateMode, CLIMATE_MODE_BITMASK_SIZE>;
using ClimateFanModeMask = EnumBitmask<ClimateFanMode, CLIMATE_FAN_MODE_BITMASK_SIZE>;
using ClimateSwingModeMask = EnumBitmask<ClimateSwingMode, CLIMATE_SWING_MODE_BITMASK_SIZE>;
using ClimatePresetMask = EnumBitmask<ClimatePreset, CLIMATE_PRESET_BITMASK_SIZE>;

}  // namespace climate
}  // namespace esphome

// Template specializations for enum-to-bit conversions
// All climate enums are sequential starting from 0, so conversions are trivial

namespace esphome {

// ClimateMode specialization (7 values: 0-6)
template<>
constexpr int EnumBitmask<climate::ClimateMode, climate::CLIMATE_MODE_BITMASK_SIZE>::enum_to_bit(
    climate::ClimateMode mode) {
  return static_cast<int>(mode);  // Direct mapping: enum value = bit position
}

template<>
inline climate::ClimateMode EnumBitmask<climate::ClimateMode, climate::CLIMATE_MODE_BITMASK_SIZE>::bit_to_enum(
    int bit) {
  // Lookup array mapping bit positions to enum values
  static constexpr climate::ClimateMode MODES[] = {
      climate::CLIMATE_MODE_OFF,        // bit 0
      climate::CLIMATE_MODE_HEAT_COOL,  // bit 1
      climate::CLIMATE_MODE_COOL,       // bit 2
      climate::CLIMATE_MODE_HEAT,       // bit 3
      climate::CLIMATE_MODE_FAN_ONLY,   // bit 4
      climate::CLIMATE_MODE_DRY,        // bit 5
      climate::CLIMATE_MODE_AUTO,       // bit 6
  };
  static constexpr int MODE_COUNT = 7;
  return (bit >= 0 && bit < MODE_COUNT) ? MODES[bit] : climate::CLIMATE_MODE_OFF;
}

// ClimateFanMode specialization (10 values: 0-9)
template<>
constexpr int EnumBitmask<climate::ClimateFanMode, climate::CLIMATE_FAN_MODE_BITMASK_SIZE>::enum_to_bit(
    climate::ClimateFanMode mode) {
  return static_cast<int>(mode);  // Direct mapping: enum value = bit position
}

template<>
inline climate::ClimateFanMode EnumBitmask<climate::ClimateFanMode,
                                           climate::CLIMATE_FAN_MODE_BITMASK_SIZE>::bit_to_enum(int bit) {
  static constexpr climate::ClimateFanMode MODES[] = {
      climate::CLIMATE_FAN_ON,       // bit 0
      climate::CLIMATE_FAN_OFF,      // bit 1
      climate::CLIMATE_FAN_AUTO,     // bit 2
      climate::CLIMATE_FAN_LOW,      // bit 3
      climate::CLIMATE_FAN_MEDIUM,   // bit 4
      climate::CLIMATE_FAN_HIGH,     // bit 5
      climate::CLIMATE_FAN_MIDDLE,   // bit 6
      climate::CLIMATE_FAN_FOCUS,    // bit 7
      climate::CLIMATE_FAN_DIFFUSE,  // bit 8
      climate::CLIMATE_FAN_QUIET,    // bit 9
  };
  static constexpr int MODE_COUNT = 10;
  return (bit >= 0 && bit < MODE_COUNT) ? MODES[bit] : climate::CLIMATE_FAN_ON;
}

// ClimateSwingMode specialization (4 values: 0-3)
template<>
constexpr int EnumBitmask<climate::ClimateSwingMode, climate::CLIMATE_SWING_MODE_BITMASK_SIZE>::enum_to_bit(
    climate::ClimateSwingMode mode) {
  return static_cast<int>(mode);  // Direct mapping: enum value = bit position
}

template<>
inline climate::ClimateSwingMode EnumBitmask<climate::ClimateSwingMode,
                                             climate::CLIMATE_SWING_MODE_BITMASK_SIZE>::bit_to_enum(int bit) {
  static constexpr climate::ClimateSwingMode MODES[] = {
      climate::CLIMATE_SWING_OFF,         // bit 0
      climate::CLIMATE_SWING_BOTH,        // bit 1
      climate::CLIMATE_SWING_VERTICAL,    // bit 2
      climate::CLIMATE_SWING_HORIZONTAL,  // bit 3
  };
  static constexpr int MODE_COUNT = 4;
  return (bit >= 0 && bit < MODE_COUNT) ? MODES[bit] : climate::CLIMATE_SWING_OFF;
}

// ClimatePreset specialization (8 values: 0-7)
template<>
constexpr int EnumBitmask<climate::ClimatePreset, climate::CLIMATE_PRESET_BITMASK_SIZE>::enum_to_bit(
    climate::ClimatePreset preset) {
  return static_cast<int>(preset);  // Direct mapping: enum value = bit position
}

template<>
inline climate::ClimatePreset EnumBitmask<climate::ClimatePreset, climate::CLIMATE_PRESET_BITMASK_SIZE>::bit_to_enum(
    int bit) {
  static constexpr climate::ClimatePreset PRESETS[] = {
      climate::CLIMATE_PRESET_NONE,      // bit 0
      climate::CLIMATE_PRESET_HOME,      // bit 1
      climate::CLIMATE_PRESET_AWAY,      // bit 2
      climate::CLIMATE_PRESET_BOOST,     // bit 3
      climate::CLIMATE_PRESET_COMFORT,   // bit 4
      climate::CLIMATE_PRESET_ECO,       // bit 5
      climate::CLIMATE_PRESET_SLEEP,     // bit 6
      climate::CLIMATE_PRESET_ACTIVITY,  // bit 7
  };
  static constexpr int PRESET_COUNT = 8;
  return (bit >= 0 && bit < PRESET_COUNT) ? PRESETS[bit] : climate::CLIMATE_PRESET_NONE;
}

}  // namespace esphome
