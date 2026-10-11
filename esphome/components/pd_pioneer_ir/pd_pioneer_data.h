#pragma once

#include "esphome/components/remote_base/pd_pioneer_protocol.h"
#include "esphome/components/climate/climate_mode.h"

namespace esphome::pd_pioneer_ir {

const uint8_t PDPIONEER_TEMPC_MIN = 16;
const uint8_t PDPIONEER_TEMPC_MAX = 31;

using climate::ClimateMode;
using climate::ClimateFanMode;
using remote_base::PDPioneerData;

class ControlData {
 public:
  ControlData();

  void set_temp(float temp_c);
  float get_temp() const;

  void set_mode(ClimateMode mode);
  ClimateMode get_mode() const;

  void set_fan_mode(ClimateFanMode mode);
  ClimateFanMode get_fan_mode() const;

  void set_swing_mode(climate::ClimateSwingMode mode);
  climate::ClimateSwingMode get_swing_mode() const;

  void set_eco(bool enabled);
  bool get_eco() const;

  void finalize();
  const PDPioneerData &odd() const { return this->odd_; }
  const PDPioneerData &even() const { return this->even_; }

  /// Apply fields from a received odd burst (fan / swing).
  void apply_odd(const PDPioneerData &data);
  /// Apply fields from a received even burst (mode / temp / power).
  void apply_even(const PDPioneerData &data);

 protected:
  static const uint8_t MODE_HEAT = 0x01;
  static const uint8_t MODE_DRY = 0x02;
  static const uint8_t MODE_COOL = 0x03;
  static const uint8_t MODE_FAN_ONLY = 0x07;
  static const uint8_t MODE_AUTO = 0x08;

  static const uint8_t PWR_ON = 0x24;
  static const uint8_t PWR_OFF = 0xA0;

  static const uint8_t SWING_OFF = 0x00;
  static const uint8_t SWING_VERTICAL = 0x08;
  static const uint8_t SWING_HORIZONTAL = 0x90;
  /// Vertical swing also sets this mask on even byte 8 (fan sub-code).
  static const uint8_t SWING_VERTICAL_EVEN_MASK = 0x38;

  void set_power_(bool on);
  bool get_power_() const;

  void set_fan_from_odd_(uint8_t byte5, uint8_t byte6);
  void sync_even_fan_byte_();

  PDPioneerData odd_;
  PDPioneerData even_;
};

}  // namespace esphome::pd_pioneer_ir
