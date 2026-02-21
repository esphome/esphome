#include "climate_mode.h"
#include "esphome/core/progmem.h"

namespace esphome::climate {

// Climate mode strings indexed by ClimateMode enum (0-6): OFF, HEAT_COOL, COOL, HEAT, FAN_ONLY, DRY, AUTO
PROGMEM_STRING_TABLE(ClimateModeStrings, "OFF", "HEAT_COOL", "COOL", "HEAT", "FAN_ONLY", "DRY", "AUTO", "UNKNOWN");

const LogString *climate_mode_to_string(ClimateMode mode) {
  return ClimateModeStrings::get_log_str(static_cast<uint8_t>(mode), ClimateModeStrings::LAST_INDEX);
}

// Climate action strings indexed by ClimateAction enum (0,2-6): OFF, (gap), COOLING, HEATING, IDLE, DRYING, FAN
PROGMEM_STRING_TABLE(ClimateActionStrings, "OFF", "UNKNOWN", "COOLING", "HEATING", "IDLE", "DRYING", "FAN", "UNKNOWN");

const LogString *climate_action_to_string(ClimateAction action) {
  return ClimateActionStrings::get_log_str(static_cast<uint8_t>(action), ClimateActionStrings::LAST_INDEX);
}

// Climate fan mode strings indexed by ClimateFanMode enum (0-9): ON, OFF, AUTO, LOW, MEDIUM, HIGH, MIDDLE, FOCUS,
// DIFFUSE, QUIET
PROGMEM_STRING_TABLE(ClimateFanModeStrings, "ON", "OFF", "AUTO", "LOW", "MEDIUM", "HIGH", "MIDDLE", "FOCUS", "DIFFUSE",
                     "QUIET", "UNKNOWN");

const LogString *climate_fan_mode_to_string(ClimateFanMode fan_mode) {
  return ClimateFanModeStrings::get_log_str(static_cast<uint8_t>(fan_mode), ClimateFanModeStrings::LAST_INDEX);
}

// Climate swing mode strings indexed by ClimateSwingMode enum (0-3): OFF, BOTH, VERTICAL, HORIZONTAL
PROGMEM_STRING_TABLE(ClimateSwingModeStrings, "OFF", "BOTH", "VERTICAL", "HORIZONTAL", "UNKNOWN");

const LogString *climate_swing_mode_to_string(ClimateSwingMode swing_mode) {
  return ClimateSwingModeStrings::get_log_str(static_cast<uint8_t>(swing_mode), ClimateSwingModeStrings::LAST_INDEX);
}

// Climate preset strings indexed by ClimatePreset enum (0-7): NONE, HOME, AWAY, BOOST, COMFORT, ECO, SLEEP, ACTIVITY
PROGMEM_STRING_TABLE(ClimatePresetStrings, "NONE", "HOME", "AWAY", "BOOST", "COMFORT", "ECO", "SLEEP", "ACTIVITY",
                     "UNKNOWN");

const LogString *climate_preset_to_string(ClimatePreset preset) {
  return ClimatePresetStrings::get_log_str(static_cast<uint8_t>(preset), ClimatePresetStrings::LAST_INDEX);
}

}  // namespace esphome::climate
