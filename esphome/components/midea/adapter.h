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
  static MideaMode from_mode(ClimateMode mode);
  static ClimateMode to_mode(MideaMode mode);

  static MideaSwingMode from_swing_mode(ClimateSwingMode mode);
  static ClimateSwingMode to_swing_mode(MideaSwingMode mode);

  static MideaPreset from_preset(ClimatePreset preset);
  static MideaPreset from_custom_preset(const std::string &preset);
  static bool is_custom_preset(MideaPreset preset);
  static ClimatePreset to_preset(MideaPreset preset);
  static const std::string &to_custom_preset(MideaPreset preset);

  static MideaFanMode from_fan_mode(ClimateFanMode fan_mode);
  static MideaFanMode from_custom_fan_mode(const std::string &fan_mode);
  static bool is_custom_fan_mode(MideaFanMode fan_mode);
  static ClimateFanMode to_fan_mode(MideaFanMode fan_mode);
  static const std::string &to_custom_fan_mode(MideaFanMode fan_mode);
};

}  // namespace midea_ac
}  // namespace esphome
