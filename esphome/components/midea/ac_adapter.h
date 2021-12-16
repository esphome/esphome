#pragma once

#ifdef USE_ARDUINO

// MideaUART
#include <Appliance/AirConditioner/AirConditioner.h>

#include "esphome/components/climate/climate_traits.h"
#include "air_conditioner.h"

namespace esphome {
namespace midea {
namespace ac {

using MideaMode = dudanov::midea::ac::Mode;
using MideaSwingMode = dudanov::midea::ac::SwingMode;
using MideaFanMode = dudanov::midea::ac::FanMode;
using MideaPreset = dudanov::midea::ac::Preset;

class Constants {
 public:
  static const char *const TAG;
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
  static void to_climate_traits(ClimateTraits &traits, const dudanov::midea::ac::Capabilities &capabilities);
};

}  // namespace ac
}  // namespace midea
}  // namespace esphome

#endif  // USE_ARDUINO
