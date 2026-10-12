#pragma once

#include "esphome/components/remote_base/pd_pioneer_protocol.h"
#include "esphome/components/climate/climate_mode.h"
#include "esphome/core/helpers.h"

namespace esphome::pd_pioneer_ir {

constexpr uint8_t PDPIONEER_TEMPC_MIN = 16;
constexpr uint8_t PDPIONEER_TEMPC_MAX = 31;

using climate::ClimateMode;
using climate::ClimateFanMode;
using remote_base::PDPioneerData;

class ControlData {
 public:
  ControlData();
  void set_temp(float temp_c);
  void set_mode(ClimateMode mode);
  void set_fan_mode(ClimateFanMode mode) { this->fan_ = mode; }
  void set_swing_mode(climate::ClimateSwingMode mode);
  void set_eco(bool enabled) {
    if (enabled)
      this->even_[5] = PWR_ECO;
  }

  /// Write the fan and swing positions, which share bytes, then the checksums
  void finalize();
  const PDPioneerData &odd() const { return this->odd_; }
  const PDPioneerData &even() const { return this->even_; }

  /// Clamped to the set point range, so a frame from a sibling remote cannot publish an impossible value
  static float get_temp(const PDPioneerData &even) {
    const float temp = ((even[12] & 0x04) ? 31.5f : 31.0f) - static_cast<float>(even[7]);
    return clamp(temp, static_cast<float>(PDPIONEER_TEMPC_MIN), static_cast<float>(PDPIONEER_TEMPC_MAX));
  }
  /// Empty for a mode byte this remote does not use
  static optional<ClimateMode> get_mode(const PDPioneerData &even);
  static bool get_eco(const PDPioneerData &even) { return even[5] == PWR_ECO; }
  static ClimateFanMode get_fan_mode(const PDPioneerData &even);
  /// Only the odd burst carries horizontal swing; the even burst carries vertical swing
  static bool get_horizontal_swing(const PDPioneerData &odd) { return (odd[7] & SWING_HORIZONTAL) == SWING_HORIZONTAL; }
  static climate::ClimateSwingMode get_swing_mode(const PDPioneerData &even, bool horizontal);

 protected:
  static constexpr uint8_t MODE_HEAT = 0x01;
  static constexpr uint8_t MODE_DRY = 0x02;
  static constexpr uint8_t MODE_COOL = 0x03;
  static constexpr uint8_t MODE_FAN_ONLY = 0x07;
  static constexpr uint8_t MODE_AUTO = 0x08;

  static constexpr uint8_t PWR_ON = 0x24;
  static constexpr uint8_t PWR_ECO = 0x25;
  static constexpr uint8_t PWR_OFF = 0xA0;

  static constexpr uint8_t SWING_VERTICAL = 0x08;
  static constexpr uint8_t SWING_HORIZONTAL = 0x90;
  /// Vertical swing also sets this mask on even frame position 8 (fan sub-code).
  static constexpr uint8_t SWING_VERTICAL_EVEN_MASK = 0x38;

  PDPioneerData odd_;
  PDPioneerData even_;
  ClimateFanMode fan_{climate::CLIMATE_FAN_HIGH};
  /// Odd-burst position 7: SWING_VERTICAL and SWING_HORIZONTAL flags
  uint8_t swing_{0};
};

}  // namespace esphome::pd_pioneer_ir
