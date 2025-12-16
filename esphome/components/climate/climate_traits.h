#pragma once

#include <cstring>
#include <vector>
#include "climate_mode.h"
#include "esphome/core/finite_set_mask.h"
#include "esphome/core/helpers.h"

namespace esphome::climate {

// Type aliases for climate enum bitmasks
// These replace std::set<EnumType> to eliminate red-black tree overhead
// For contiguous enums starting at 0, DefaultBitPolicy provides 1:1 mapping (enum value = bit position)
// Bitmask size is automatically calculated from the last enum value
using ClimateModeMask = FiniteSetMask<ClimateMode, DefaultBitPolicy<ClimateMode, CLIMATE_MODE_AUTO + 1>>;
using ClimateFanModeMask = FiniteSetMask<ClimateFanMode, DefaultBitPolicy<ClimateFanMode, CLIMATE_FAN_QUIET + 1>>;
using ClimateSwingModeMask =
    FiniteSetMask<ClimateSwingMode, DefaultBitPolicy<ClimateSwingMode, CLIMATE_SWING_HORIZONTAL + 1>>;
using ClimatePresetMask = FiniteSetMask<ClimatePreset, DefaultBitPolicy<ClimatePreset, CLIMATE_PRESET_ACTIVITY + 1>>;

// Lightweight linear search for small vectors (1-20 items) of const char* pointers
// Avoids std::find template overhead
inline bool vector_contains(const std::vector<const char *> &vec, const char *value) {
  for (const char *item : vec) {
    if (strcmp(item, value) == 0)
      return true;
  }
  return false;
}

// Find and return matching pointer from vector, or nullptr if not found
inline const char *vector_find(const std::vector<const char *> &vec, const char *value) {
  for (const char *item : vec) {
    if (strcmp(item, value) == 0)
      return item;
  }
  return nullptr;
}

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
class Climate;  // Forward declaration

class ClimateTraits {
  friend class Climate;  // Allow Climate to access protected find methods

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
  bool supports_fan_mode(ClimateFanMode fan_mode) const { return this->supported_fan_modes_.count(fan_mode); }
  bool get_supports_fan_modes() const {
    return !this->supported_fan_modes_.empty() || !this->supported_custom_fan_modes_.empty();
  }
  const ClimateFanModeMask &get_supported_fan_modes() const { return this->supported_fan_modes_; }

  void set_supported_custom_fan_modes(std::initializer_list<const char *> modes) {
    this->supported_custom_fan_modes_ = modes;
  }
  void set_supported_custom_fan_modes(const std::vector<const char *> &modes) {
    this->supported_custom_fan_modes_ = modes;
  }
  template<size_t N> void set_supported_custom_fan_modes(const char *const (&modes)[N]) {
    this->supported_custom_fan_modes_.assign(modes, modes + N);
  }

  // Deleted overloads to catch incorrect std::string usage at compile time with clear error messages
  void set_supported_custom_fan_modes(const std::vector<std::string> &modes) = delete;
  void set_supported_custom_fan_modes(std::initializer_list<std::string> modes) = delete;

  const std::vector<const char *> &get_supported_custom_fan_modes() const { return this->supported_custom_fan_modes_; }
  bool supports_custom_fan_mode(const char *custom_fan_mode) const {
    return vector_contains(this->supported_custom_fan_modes_, custom_fan_mode);
  }
  bool supports_custom_fan_mode(const std::string &custom_fan_mode) const {
    return this->supports_custom_fan_mode(custom_fan_mode.c_str());
  }

  void set_supported_presets(ClimatePresetMask presets) { this->supported_presets_ = presets; }
  void add_supported_preset(ClimatePreset preset) { this->supported_presets_.insert(preset); }
  bool supports_preset(ClimatePreset preset) const { return this->supported_presets_.count(preset); }
  bool get_supports_presets() const { return !this->supported_presets_.empty(); }
  const ClimatePresetMask &get_supported_presets() const { return this->supported_presets_; }

  void set_supported_custom_presets(std::initializer_list<const char *> presets) {
    this->supported_custom_presets_ = presets;
  }
  void set_supported_custom_presets(const std::vector<const char *> &presets) {
    this->supported_custom_presets_ = presets;
  }
  template<size_t N> void set_supported_custom_presets(const char *const (&presets)[N]) {
    this->supported_custom_presets_.assign(presets, presets + N);
  }

  // Deleted overloads to catch incorrect std::string usage at compile time with clear error messages
  void set_supported_custom_presets(const std::vector<std::string> &presets) = delete;
  void set_supported_custom_presets(std::initializer_list<std::string> presets) = delete;

  const std::vector<const char *> &get_supported_custom_presets() const { return this->supported_custom_presets_; }
  bool supports_custom_preset(const char *custom_preset) const {
    return vector_contains(this->supported_custom_presets_, custom_preset);
  }
  bool supports_custom_preset(const std::string &custom_preset) const {
    return this->supports_custom_preset(custom_preset.c_str());
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

  /// Find and return the matching custom fan mode pointer from supported modes, or nullptr if not found
  /// This is protected as it's an implementation detail - use Climate::find_custom_fan_mode_() instead
  const char *find_custom_fan_mode_(const char *custom_fan_mode) const {
    return vector_find(this->supported_custom_fan_modes_, custom_fan_mode);
  }

  /// Find and return the matching custom preset pointer from supported presets, or nullptr if not found
  /// This is protected as it's an implementation detail - use Climate::find_custom_preset_() instead
  const char *find_custom_preset_(const char *custom_preset) const {
    return vector_find(this->supported_custom_presets_, custom_preset);
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

  /** Custom mode storage using const char* pointers to eliminate std::string overhead.
   *
   * Pointers must remain valid for the ClimateTraits lifetime. Safe patterns:
   *  - String literals: set_supported_custom_fan_modes({"Turbo", "Silent"})
   *  - Static const data: static const char* MODE = "Eco";
   *
   * Climate class setters validate pointers are from these vectors before storing.
   */
  std::vector<const char *> supported_custom_fan_modes_;
  std::vector<const char *> supported_custom_presets_;
};

}  // namespace esphome::climate
