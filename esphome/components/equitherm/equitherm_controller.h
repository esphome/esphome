#pragma once

#include "esphome/core/hal.h"
#include <cmath>

namespace esphome::equitherm {

/// Standard European heating curve (Buderus/Viessmann style).
/// Pure calculator — no side effects, testable in isolation.
///
/// Formula: t_flow = t_target + shift + hc * (t_target - t_outdoor)^(1/n)
///
/// Parameters:
///   hc: heat curve coefficient (0.5-1.5), maps to building insulation
///   n:  radiator exponent (1.2-1.33 panels, 1.0 underfloor) per EN 442
///   shift: flat offset in °C for fine-tuning
class HeatingCurve {
 public:
  /// Compute supply temperature from target and outdoor temperatures
  /// @param t_target Room target temperature in °C
  /// @param t_outdoor Outdoor temperature in °C
  /// @return Calculated supply temperature in °C, clamped to [min_flow_temp_, max_flow_temp_].
  ///         Returns 0.0f when delta_t <= 0 (warm weather shutdown).
  float compute_flow_temperature(float t_target, float t_outdoor);

  // Parameter setters (called from EquithermClimate)
  void set_hc(float hc) { hc_ = hc; }
  void set_n(float n) { n_ = n; }
  void set_shift(float shift) { shift_ = shift; }
  void set_min_flow_temp(float temp) { min_flow_temp_ = temp; }
  void set_max_flow_temp(float temp) { max_flow_temp_ = temp; }

  // Parameter getters (for dump_config and diagnostics)
  float get_hc() const { return hc_; }
  float get_n() const { return n_; }
  float get_shift() const { return shift_; }
  float get_min_flow_temp() const { return min_flow_temp_; }
  float get_max_flow_temp() const { return max_flow_temp_; }

 protected:
  /// Heat curve coefficient (industry: 0.5-1.5 depending on insulation)
  /// Higher values = more aggressive heating
  float hc_{1.0f};
  /// Radiator exponent (industry: 1.2-1.33 for panel radiators, 1.0 for underfloor)
  float n_{1.25f};
  /// Flat offset in °C for fine-tuning
  float shift_{0.0f};
  /// Minimum supply temperature to boiler
  float min_flow_temp_{25.0f};
  /// Maximum supply temperature to boiler
  float max_flow_temp_{70.0f};
};

}  // namespace esphome::equitherm
