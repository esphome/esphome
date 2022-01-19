#ifdef USE_ARDUINO

#include "esphome/core/log.h"
#include "ac_adapter.h"

namespace esphome {
namespace midea {
namespace ac {

const char *const Constants::TAG = "midea";
const std::string Constants::FREEZE_PROTECTION = "freeze protection";
const std::string Constants::SILENT = "silent";
const std::string Constants::TURBO = "turbo";

ClimateMode Converters::to_climate_mode(MideaMode mode) {
  switch (mode) {
    case MideaMode::MODE_AUTO:
      return ClimateMode::CLIMATE_MODE_HEAT_COOL;
    case MideaMode::MODE_COOL:
      return ClimateMode::CLIMATE_MODE_COOL;
    case MideaMode::MODE_DRY:
      return ClimateMode::CLIMATE_MODE_DRY;
    case MideaMode::MODE_FAN_ONLY:
      return ClimateMode::CLIMATE_MODE_FAN_ONLY;
    case MideaMode::MODE_HEAT:
      return ClimateMode::CLIMATE_MODE_HEAT;
    default:
      return ClimateMode::CLIMATE_MODE_OFF;
  }
}

MideaMode Converters::to_midea_mode(ClimateMode mode) {
  switch (mode) {
    case ClimateMode::CLIMATE_MODE_HEAT_COOL:
      return MideaMode::MODE_AUTO;
    case ClimateMode::CLIMATE_MODE_COOL:
      return MideaMode::MODE_COOL;
    case ClimateMode::CLIMATE_MODE_DRY:
      return MideaMode::MODE_DRY;
    case ClimateMode::CLIMATE_MODE_FAN_ONLY:
      return MideaMode::MODE_FAN_ONLY;
    case ClimateMode::CLIMATE_MODE_HEAT:
      return MideaMode::MODE_HEAT;
    default:
      return MideaMode::MODE_OFF;
  }
}

ClimateSwingMode Converters::to_climate_swing_mode(MideaSwingMode mode) {
  switch (mode) {
    case MideaSwingMode::SWING_VERTICAL:
      return ClimateSwingMode::CLIMATE_SWING_VERTICAL;
    case MideaSwingMode::SWING_HORIZONTAL:
      return ClimateSwingMode::CLIMATE_SWING_HORIZONTAL;
    case MideaSwingMode::SWING_BOTH:
      return ClimateSwingMode::CLIMATE_SWING_BOTH;
    default:
      return ClimateSwingMode::CLIMATE_SWING_OFF;
  }
}

MideaSwingMode Converters::to_midea_swing_mode(ClimateSwingMode mode) {
  switch (mode) {
    case ClimateSwingMode::CLIMATE_SWING_VERTICAL:
      return MideaSwingMode::SWING_VERTICAL;
    case ClimateSwingMode::CLIMATE_SWING_HORIZONTAL:
      return MideaSwingMode::SWING_HORIZONTAL;
    case ClimateSwingMode::CLIMATE_SWING_BOTH:
      return MideaSwingMode::SWING_BOTH;
    default:
      return MideaSwingMode::SWING_OFF;
  }
}

MideaFanMode Converters::to_midea_fan_mode(ClimateFanMode mode) {
  switch (mode) {
    case ClimateFanMode::CLIMATE_FAN_LOW:
      return MideaFanMode::FAN_LOW;
    case ClimateFanMode::CLIMATE_FAN_MEDIUM:
      return MideaFanMode::FAN_MEDIUM;
    case ClimateFanMode::CLIMATE_FAN_HIGH:
      return MideaFanMode::FAN_HIGH;
    default:
      return MideaFanMode::FAN_AUTO;
  }
}

ClimateFanMode Converters::to_climate_fan_mode(MideaFanMode mode) {
  switch (mode) {
    case MideaFanMode::FAN_LOW:
      return ClimateFanMode::CLIMATE_FAN_LOW;
    case MideaFanMode::FAN_MEDIUM:
      return ClimateFanMode::CLIMATE_FAN_MEDIUM;
    case MideaFanMode::FAN_HIGH:
      return ClimateFanMode::CLIMATE_FAN_HIGH;
    default:
      return ClimateFanMode::CLIMATE_FAN_AUTO;
  }
}

bool Converters::is_custom_midea_fan_mode(MideaFanMode mode) {
  switch (mode) {
    case MideaFanMode::FAN_SILENT:
    case MideaFanMode::FAN_TURBO:
      return true;
    default:
      return false;
  }
}

const std::string &Converters::to_custom_climate_fan_mode(MideaFanMode mode) {
  switch (mode) {
    case MideaFanMode::FAN_SILENT:
      return Constants::SILENT;
    default:
      return Constants::TURBO;
  }
}

MideaFanMode Converters::to_midea_fan_mode(const std::string &mode) {
  if (mode == Constants::SILENT)
    return MideaFanMode::FAN_SILENT;
  return MideaFanMode::FAN_TURBO;
}

MideaPreset Converters::to_midea_preset(ClimatePreset preset) {
  switch (preset) {
    case ClimatePreset::CLIMATE_PRESET_SLEEP:
      return MideaPreset::PRESET_SLEEP;
    case ClimatePreset::CLIMATE_PRESET_ECO:
      return MideaPreset::PRESET_ECO;
    case ClimatePreset::CLIMATE_PRESET_BOOST:
      return MideaPreset::PRESET_TURBO;
    default:
      return MideaPreset::PRESET_NONE;
  }
}

ClimatePreset Converters::to_climate_preset(MideaPreset preset) {
  switch (preset) {
    case MideaPreset::PRESET_SLEEP:
      return ClimatePreset::CLIMATE_PRESET_SLEEP;
    case MideaPreset::PRESET_ECO:
      return ClimatePreset::CLIMATE_PRESET_ECO;
    case MideaPreset::PRESET_TURBO:
      return ClimatePreset::CLIMATE_PRESET_BOOST;
    default:
      return ClimatePreset::CLIMATE_PRESET_NONE;
  }
}

bool Converters::is_custom_midea_preset(MideaPreset preset) { return preset == MideaPreset::PRESET_FREEZE_PROTECTION; }

const std::string &Converters::to_custom_climate_preset(MideaPreset preset) { return Constants::FREEZE_PROTECTION; }

MideaPreset Converters::to_midea_preset(const std::string &preset) { return MideaPreset::PRESET_FREEZE_PROTECTION; }

void Converters::to_climate_traits(ClimateTraits &traits, const dudanov::midea::ac::Capabilities &capabilities) {
  if (capabilities.supportAutoMode())
    traits.add_supported_mode(ClimateMode::CLIMATE_MODE_HEAT_COOL);
  if (capabilities.supportCoolMode())
    traits.add_supported_mode(ClimateMode::CLIMATE_MODE_COOL);
  if (capabilities.supportHeatMode())
    traits.add_supported_mode(ClimateMode::CLIMATE_MODE_HEAT);
  if (capabilities.supportDryMode())
    traits.add_supported_mode(ClimateMode::CLIMATE_MODE_DRY);
  if (capabilities.supportTurboPreset())
    traits.add_supported_preset(ClimatePreset::CLIMATE_PRESET_BOOST);
  if (capabilities.supportEcoPreset())
    traits.add_supported_preset(ClimatePreset::CLIMATE_PRESET_ECO);
  if (capabilities.supportFrostProtectionPreset())
    traits.add_supported_custom_preset(Constants::FREEZE_PROTECTION);
}

}  // namespace ac
}  // namespace midea
}  // namespace esphome

#endif  // USE_ARDUINO
