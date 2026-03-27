#include "climate_traits.h"

namespace esphome::climate {

// Compat: shared empty vector for getters when no custom modes are set.
// Remove in 2026.11.0 when deprecated ClimateTraits setters are removed
// and getters can return const vector * instead of const vector &.
static const std::vector<const char *> EMPTY_CUSTOM_MODES;  // NOLINT

const std::vector<const char *> &ClimateTraits::get_supported_custom_fan_modes() const {
  // Compat: return empty ref when pointer is null. Remove in 2026.11.0 (change return to const vector *).
  return this->supported_custom_fan_modes_ ? *this->supported_custom_fan_modes_ : EMPTY_CUSTOM_MODES;
}

const std::vector<const char *> &ClimateTraits::get_supported_custom_presets() const {
  // Compat: return empty ref when pointer is null. Remove in 2026.11.0 (change return to const vector *).
  return this->supported_custom_presets_ ? *this->supported_custom_presets_ : EMPTY_CUSTOM_MODES;
}

int8_t ClimateTraits::get_target_temperature_accuracy_decimals() const {
  return step_to_accuracy_decimals(this->visual_target_temperature_step_);
}

int8_t ClimateTraits::get_current_temperature_accuracy_decimals() const {
  return step_to_accuracy_decimals(this->visual_current_temperature_step_);
}

}  // namespace esphome::climate
