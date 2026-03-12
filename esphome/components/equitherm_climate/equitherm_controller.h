#pragma once

#include "esphome/core/hal.h"
#include <cmath>

namespace esphome {
namespace equitherm_climate {

/// Pure heating curve calculator - no side effects, testable in isolation
class EquithermController {
 public:
  /// Compute supply temperature from target and outdoor temperatures
  /// @param t_target Room target temperature in °C
  /// @param t_outdoor Outdoor temperature in °C
  /// @return Calculated supply temperature in °C, clamped to [t_min_flow_, t_max_flow_]
  float compute(float t_target, float t_outdoor);

  // Parameter setters (called from EquithermClimate)
  void set_slope(float slope) { slope_ = slope; }
  void set_exponent(float exponent) { exponent_ = exponent; }
  void set_shift(float shift) { shift_ = shift; }
  void set_t_min_flow(float temp) { t_min_flow_ = temp; }
  void set_t_max_flow(float temp) { t_max_flow_ = temp; }

  // Parameter getters (for dump_config and diagnostics)
  float get_slope() const { return slope_; }
  float get_exponent() const { return exponent_; }
  float get_shift() const { return shift_; }
  float get_t_min_flow() const { return t_min_flow_; }
  float get_t_max_flow() const { return t_max_flow_; }

 protected:
  /// Steepness of the heating curve (OTGateway: equitherm.slope)
  float slope_{1.5f};
  /// Curve shape exponent (OTGateway: used as 1/exponent in pow())
  float exponent_{1.5f};
  /// Flat offset in °C (OTGateway: equitherm.shift)
  float shift_{0.0f};
  /// Minimum supply temperature to boiler
  float t_min_flow_{25.0f};
  /// Maximum supply temperature to boiler
  float t_max_flow_{70.0f};
};

}  // namespace equitherm_climate
}  // namespace esphome
