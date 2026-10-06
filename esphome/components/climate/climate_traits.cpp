#include "climate_traits.h"

namespace esphome::climate {

static const ClimateCustomModes EMPTY_CUSTOM_MODES;  // NOLINT

const ClimateCustomModes &ClimateTraits::get_supported_custom_fan_modes() const {
  return this->supported_custom_fan_modes_ != nullptr ? *this->supported_custom_fan_modes_ : EMPTY_CUSTOM_MODES;
}

const ClimateCustomModes &ClimateTraits::get_supported_custom_presets() const {
  return this->supported_custom_presets_ != nullptr ? *this->supported_custom_presets_ : EMPTY_CUSTOM_MODES;
}

int8_t ClimateTraits::get_target_temperature_accuracy_decimals() const {
  return step_to_accuracy_decimals(this->visual_target_temperature_step_);
}

int8_t ClimateTraits::get_current_temperature_accuracy_decimals() const {
  return step_to_accuracy_decimals(this->visual_current_temperature_step_);
}

}  // namespace esphome::climate
