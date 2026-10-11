#pragma once
#include <initializer_list>
namespace esphome::climate {
struct ClimateCall {};
struct ClimateTraits {};
enum ClimateMode { CLIMATE_MODE_OFF };
using ClimatePreset = int;
using ClimateSwingMode = int;
using ClimateFanMode = int;
using ClimateModeMask = int;
using ClimateSwingModeMask = int;
using ClimatePresetMask = int;
class Climate {
 public:
  virtual ~Climate() = default;
  ClimateMode mode{CLIMATE_MODE_OFF};
  void set_supported_custom_presets(std::initializer_list<const char *>) {}
  void set_supported_custom_fan_modes(std::initializer_list<const char *>) {}

 protected:
  virtual void control(const ClimateCall &) = 0;
  virtual ClimateTraits traits() = 0;
};
}  // namespace esphome::climate
