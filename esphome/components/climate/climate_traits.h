#pragma once

#include <vector>
#include "climate_mode.h"
#include "esphome/core/finite_set_mask.h"
#include "esphome/core/helpers.h"

// Forward declare climate enums and bitmask sizes
namespace esphome::climate {
constexpr int CLIMATE_MODE_BITMASK_SIZE = 8;  // 7 values (OFF, HEAT_COOL, COOL, HEAT, FAN_ONLY, DRY, AUTO)
constexpr int CLIMATE_FAN_MODE_BITMASK_SIZE =
    16;  // 10 values (ON, OFF, AUTO, LOW, MEDIUM, HIGH, MIDDLE, FOCUS, DIFFUSE, QUIET)
constexpr int CLIMATE_SWING_MODE_BITMASK_SIZE = 8;  // 4 values (OFF, BOTH, VERTICAL, HORIZONTAL)
constexpr int CLIMATE_PRESET_BITMASK_SIZE = 8;      // 8 values (NONE, HOME, AWAY, BOOST, COMFORT, ECO, SLEEP, ACTIVITY)
}  // namespace esphome::climate

// Template specializations for value-to-bit conversions
// MUST be declared before any instantiation of FiniteSetMask<ClimateMode>, etc.
namespace esphome {

// ClimateMode uses 1:1 mapping (value_to_bit is just a cast)
// Only bit_to_value needs specialization
template<>
inline climate::ClimateMode FiniteSetMask<climate::ClimateMode, climate::CLIMATE_MODE_BITMASK_SIZE>::bit_to_value(
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

// ClimateFanMode uses 1:1 mapping (value_to_bit is just a cast)
// Only bit_to_value needs specialization
template<>
inline climate::ClimateFanMode FiniteSetMask<climate::ClimateFanMode,
                                             climate::CLIMATE_FAN_MODE_BITMASK_SIZE>::bit_to_value(int bit) {
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

// ClimateSwingMode uses 1:1 mapping (value_to_bit is just a cast)
// Only bit_to_value needs specialization
template<>
inline climate::ClimateSwingMode FiniteSetMask<climate::ClimateSwingMode,
                                               climate::CLIMATE_SWING_MODE_BITMASK_SIZE>::bit_to_value(int bit) {
  static constexpr climate::ClimateSwingMode MODES[] = {
      climate::CLIMATE_SWING_OFF,         // bit 0
      climate::CLIMATE_SWING_BOTH,        // bit 1
      climate::CLIMATE_SWING_VERTICAL,    // bit 2
      climate::CLIMATE_SWING_HORIZONTAL,  // bit 3
  };
  static constexpr int MODE_COUNT = 4;
  return (bit >= 0 && bit < MODE_COUNT) ? MODES[bit] : climate::CLIMATE_SWING_OFF;
}

// ClimatePreset uses 1:1 mapping (value_to_bit is just a cast)
// Only bit_to_value needs specialization
template<>
inline climate::ClimatePreset FiniteSetMask<climate::ClimatePreset, climate::CLIMATE_PRESET_BITMASK_SIZE>::bit_to_value(
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

// Now we can safely create the type aliases
namespace esphome::climate {

// Type aliases for climate enum bitmasks
// These replace std::set<EnumType> to eliminate red-black tree overhead
using ClimateModeMask = FiniteSetMask<ClimateMode, CLIMATE_MODE_BITMASK_SIZE>;
using ClimateFanModeMask = FiniteSetMask<ClimateFanMode, CLIMATE_FAN_MODE_BITMASK_SIZE>;
using ClimateSwingModeMask = FiniteSetMask<ClimateSwingMode, CLIMATE_SWING_MODE_BITMASK_SIZE>;
using ClimatePresetMask = FiniteSetMask<ClimatePreset, CLIMATE_PRESET_BITMASK_SIZE>;

// Lightweight linear search for small vectors (1-20 items)
// Avoids std::find template overhead
template<typename T> inline bool vector_contains(const std::vector<T> &vec, const T &value) {
  for (const auto &item : vec) {
    if (item == value)
      return true;
  }
  return false;
}

}  // namespace esphome::climate

namespace esphome {

#ifdef USE_API
namespace api {
class APIConnection;
}  // namespace api
#endif

namespace climate {

/** This class contains all static data for climate devices.
 *
 * All climate devices must support these features:
 *  - OFF mode
 *  - Target Temperature
 *
 * All other properties and modes are optional and the integration must mark
 * each of them as supported by setting the appropriate flag(s) here.
 *
 *  - feature flags: see ClimateFeatures enum in climate_mode.h
 *  - supports modes:
 *    - auto mode (automatic control)
 *    - cool mode (lowers current temperature)
 *    - heat mode (increases current temperature)
 *    - dry mode (removes humidity from air)
 *    - fan mode (only turns on fan)
 *  - supports fan modes - optionally, if it has a fan which can be configured in different ways:
 *    - on, off, auto, high, medium, low, middle, focus, diffuse, quiet
 *  - supports swing modes - optionally, if it has a swing which can be configured in different ways:
 *    - off, both, vertical, horizontal
 *
 * This class also contains static data for the climate device display:
 *  - visual min/max temperature/humidity - tells the frontend what range of temperature/humidity the
 *     climate device should display (gauge min/max values)
 *  - temperature step - the step with which to increase/decrease target temperature.
 *     This also affects with how many decimal places the temperature is shown
 */
class ClimateTraits {
 public:
  /// Get/set feature flags (see ClimateFeatures enum in climate_mode.h)
  uint32_t get_feature_flags() const { return this->feature_flags_; }
  void add_feature_flags(uint32_t feature_flags) { this->feature_flags_ |= feature_flags; }
  void clear_feature_flags(uint32_t feature_flags) { this->feature_flags_ &= ~feature_flags; }
  bool has_feature_flags(uint32_t feature_flags) const { return this->feature_flags_ & feature_flags; }
  void set_feature_flags(uint32_t feature_flags) { this->feature_flags_ = feature_flags; }

  ESPDEPRECATED("This method is deprecated, use get_feature_flags() instead", "2025.11.0")
  bool get_supports_current_temperature() const {
    return this->has_feature_flags(CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
  }
  ESPDEPRECATED("This method is deprecated, use add_feature_flags() instead", "2025.11.0")
  void set_supports_current_temperature(bool supports_current_temperature) {
    if (supports_current_temperature) {
      this->add_feature_flags(CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
    } else {
      this->clear_feature_flags(CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
    }
  }
  ESPDEPRECATED("This method is deprecated, use get_feature_flags() instead", "2025.11.0")
  bool get_supports_current_humidity() const { return this->has_feature_flags(CLIMATE_SUPPORTS_CURRENT_HUMIDITY); }
  ESPDEPRECATED("This method is deprecated, use add_feature_flags() instead", "2025.11.0")
  void set_supports_current_humidity(bool supports_current_humidity) {
    if (supports_current_humidity) {
      this->add_feature_flags(CLIMATE_SUPPORTS_CURRENT_HUMIDITY);
    } else {
      this->clear_feature_flags(CLIMATE_SUPPORTS_CURRENT_HUMIDITY);
    }
  }
  ESPDEPRECATED("This method is deprecated, use get_feature_flags() instead", "2025.11.0")
  bool get_supports_two_point_target_temperature() const {
    return this->has_feature_flags(CLIMATE_REQUIRES_TWO_POINT_TARGET_TEMPERATURE);
  }
  ESPDEPRECATED("This method is deprecated, use add_feature_flags() instead", "2025.11.0")
  void set_supports_two_point_target_temperature(bool supports_two_point_target_temperature) {
    if (supports_two_point_target_temperature)
    // Use CLIMATE_REQUIRES_TWO_POINT_TARGET_TEMPERATURE to mimic previous behavior
    {
      this->add_feature_flags(CLIMATE_REQUIRES_TWO_POINT_TARGET_TEMPERATURE);
    } else {
      this->clear_feature_flags(CLIMATE_REQUIRES_TWO_POINT_TARGET_TEMPERATURE);
    }
  }
  ESPDEPRECATED("This method is deprecated, use get_feature_flags() instead", "2025.11.0")
  bool get_supports_target_humidity() const { return this->has_feature_flags(CLIMATE_SUPPORTS_TARGET_HUMIDITY); }
  ESPDEPRECATED("This method is deprecated, use add_feature_flags() instead", "2025.11.0")
  void set_supports_target_humidity(bool supports_target_humidity) {
    if (supports_target_humidity) {
      this->add_feature_flags(CLIMATE_SUPPORTS_TARGET_HUMIDITY);
    } else {
      this->clear_feature_flags(CLIMATE_SUPPORTS_TARGET_HUMIDITY);
    }
  }
  ESPDEPRECATED("This method is deprecated, use get_feature_flags() instead", "2025.11.0")
  bool get_supports_action() const { return this->has_feature_flags(CLIMATE_SUPPORTS_ACTION); }
  ESPDEPRECATED("This method is deprecated, use add_feature_flags() instead", "2025.11.0")
  void set_supports_action(bool supports_action) {
    if (supports_action) {
      this->add_feature_flags(CLIMATE_SUPPORTS_ACTION);
    } else {
      this->clear_feature_flags(CLIMATE_SUPPORTS_ACTION);
    }
  }

  void set_supported_modes(ClimateModeMask modes) { this->supported_modes_ = modes; }
  void add_supported_mode(ClimateMode mode) { this->supported_modes_.insert(mode); }
  bool supports_mode(ClimateMode mode) const { return this->supported_modes_.count(mode); }
  const ClimateModeMask &get_supported_modes() const { return this->supported_modes_; }

  void set_supported_fan_modes(ClimateFanModeMask modes) { this->supported_fan_modes_ = modes; }
  void add_supported_fan_mode(ClimateFanMode mode) { this->supported_fan_modes_.insert(mode); }
  void add_supported_custom_fan_mode(const std::string &mode) { this->supported_custom_fan_modes_.push_back(mode); }
  bool supports_fan_mode(ClimateFanMode fan_mode) const { return this->supported_fan_modes_.count(fan_mode); }
  bool get_supports_fan_modes() const {
    return !this->supported_fan_modes_.empty() || !this->supported_custom_fan_modes_.empty();
  }
  const ClimateFanModeMask &get_supported_fan_modes() const { return this->supported_fan_modes_; }

  void set_supported_custom_fan_modes(std::vector<std::string> supported_custom_fan_modes) {
    this->supported_custom_fan_modes_ = std::move(supported_custom_fan_modes);
  }
  void set_supported_custom_fan_modes(std::initializer_list<std::string> modes) {
    this->supported_custom_fan_modes_ = modes;
  }
  template<size_t N> void set_supported_custom_fan_modes(const char *const (&modes)[N]) {
    this->supported_custom_fan_modes_.assign(modes, modes + N);
  }
  const std::vector<std::string> &get_supported_custom_fan_modes() const { return this->supported_custom_fan_modes_; }
  bool supports_custom_fan_mode(const std::string &custom_fan_mode) const {
    return vector_contains(this->supported_custom_fan_modes_, custom_fan_mode);
  }

  void set_supported_presets(ClimatePresetMask presets) { this->supported_presets_ = presets; }
  void add_supported_preset(ClimatePreset preset) { this->supported_presets_.insert(preset); }
  void add_supported_custom_preset(const std::string &preset) { this->supported_custom_presets_.push_back(preset); }
  bool supports_preset(ClimatePreset preset) const { return this->supported_presets_.count(preset); }
  bool get_supports_presets() const { return !this->supported_presets_.empty(); }
  const ClimatePresetMask &get_supported_presets() const { return this->supported_presets_; }

  void set_supported_custom_presets(std::vector<std::string> supported_custom_presets) {
    this->supported_custom_presets_ = std::move(supported_custom_presets);
  }
  void set_supported_custom_presets(std::initializer_list<std::string> presets) {
    this->supported_custom_presets_ = presets;
  }
  template<size_t N> void set_supported_custom_presets(const char *const (&presets)[N]) {
    this->supported_custom_presets_.assign(presets, presets + N);
  }
  const std::vector<std::string> &get_supported_custom_presets() const { return this->supported_custom_presets_; }
  bool supports_custom_preset(const std::string &custom_preset) const {
    return vector_contains(this->supported_custom_presets_, custom_preset);
  }

  void set_supported_swing_modes(ClimateSwingModeMask modes) { this->supported_swing_modes_ = modes; }
  void add_supported_swing_mode(ClimateSwingMode mode) { this->supported_swing_modes_.insert(mode); }
  bool supports_swing_mode(ClimateSwingMode swing_mode) const { return this->supported_swing_modes_.count(swing_mode); }
  bool get_supports_swing_modes() const { return !this->supported_swing_modes_.empty(); }
  const ClimateSwingModeMask &get_supported_swing_modes() const { return this->supported_swing_modes_; }

  float get_visual_min_temperature() const { return this->visual_min_temperature_; }
  void set_visual_min_temperature(float visual_min_temperature) {
    this->visual_min_temperature_ = visual_min_temperature;
  }
  float get_visual_max_temperature() const { return this->visual_max_temperature_; }
  void set_visual_max_temperature(float visual_max_temperature) {
    this->visual_max_temperature_ = visual_max_temperature;
  }
  float get_visual_target_temperature_step() const { return this->visual_target_temperature_step_; }
  float get_visual_current_temperature_step() const { return this->visual_current_temperature_step_; }
  void set_visual_target_temperature_step(float temperature_step) {
    this->visual_target_temperature_step_ = temperature_step;
  }
  void set_visual_current_temperature_step(float temperature_step) {
    this->visual_current_temperature_step_ = temperature_step;
  }
  void set_visual_temperature_step(float temperature_step) {
    this->visual_target_temperature_step_ = temperature_step;
    this->visual_current_temperature_step_ = temperature_step;
  }
  int8_t get_target_temperature_accuracy_decimals() const;
  int8_t get_current_temperature_accuracy_decimals() const;

  float get_visual_min_humidity() const { return this->visual_min_humidity_; }
  void set_visual_min_humidity(float visual_min_humidity) { this->visual_min_humidity_ = visual_min_humidity; }
  float get_visual_max_humidity() const { return this->visual_max_humidity_; }
  void set_visual_max_humidity(float visual_max_humidity) { this->visual_max_humidity_ = visual_max_humidity; }

 protected:
  void set_mode_support_(climate::ClimateMode mode, bool supported) {
    if (supported) {
      this->supported_modes_.insert(mode);
    } else {
      this->supported_modes_.erase(mode);
    }
  }
  void set_fan_mode_support_(climate::ClimateFanMode mode, bool supported) {
    if (supported) {
      this->supported_fan_modes_.insert(mode);
    } else {
      this->supported_fan_modes_.erase(mode);
    }
  }
  void set_swing_mode_support_(climate::ClimateSwingMode mode, bool supported) {
    if (supported) {
      this->supported_swing_modes_.insert(mode);
    } else {
      this->supported_swing_modes_.erase(mode);
    }
  }

  uint32_t feature_flags_{0};
  float visual_min_temperature_{10};
  float visual_max_temperature_{30};
  float visual_target_temperature_step_{0.1};
  float visual_current_temperature_step_{0.1};
  float visual_min_humidity_{30};
  float visual_max_humidity_{99};

  climate::ClimateModeMask supported_modes_{climate::CLIMATE_MODE_OFF};
  climate::ClimateFanModeMask supported_fan_modes_;
  climate::ClimateSwingModeMask supported_swing_modes_;
  climate::ClimatePresetMask supported_presets_;
  std::vector<std::string> supported_custom_fan_modes_;
  std::vector<std::string> supported_custom_presets_;
};

}  // namespace climate
}  // namespace esphome
