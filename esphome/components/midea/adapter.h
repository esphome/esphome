#pragma once
#include "esphome/components/midea/appliance_base.h"

namespace esphome {
namespace midea {

class Constants {
 public:
  static const char *TAG;
  static const std::string FREEZE_PROTECTION;
  static const std::string SILENT;
  static const std::string TURBO;
};

class Converters {
 public:
  static MideaMode to_midea_mode(ClimateMode mode);
  static ClimateMode to_climate_mode(MideaMode mode);
  static MideaSwingMode to_midea_swing_mode(ClimateSwingMode mode);
  static ClimateSwingMode to_climate_swing_mode(MideaSwingMode mode);
  static MideaPreset to_midea_preset(ClimatePreset preset);
  static MideaPreset to_midea_preset(const std::string &preset);
  static bool is_custom_midea_preset(MideaPreset preset);
  static ClimatePreset to_climate_preset(MideaPreset preset);
  static const std::string &to_custom_climate_preset(MideaPreset preset);
  static MideaFanMode to_midea_fan_mode(ClimateFanMode fan_mode);
  static MideaFanMode to_midea_fan_mode(const std::string &fan_mode);
  static bool is_custom_midea_fan_mode(MideaFanMode fan_mode);
  static ClimateFanMode to_climate_fan_mode(MideaFanMode fan_mode);
  static const std::string &to_custom_climate_fan_mode(MideaFanMode fan_mode);
};

}  // namespace midea_ac
}  // namespace esphome
