#include "equitherm_controller.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace equitherm {

float HeatingCurve::compute_flow_temperature(float t_target, float t_outdoor) {
  // Guard against invalid inputs
  if (std::isnan(t_target) || std::isnan(t_outdoor)) {
    return min_flow_temp_;
  }

  float delta_t = t_target - t_outdoor;

  // Only apply heating curve when outdoor is below target (heating needed)
  // When delta_t <= 0, return min_flow_temp to signal "no heating needed"
  float t_flow;
  if (delta_t > 0.0f) {
    // Industry-standard European heating curve:
    // t_flow = t_target + shift + hc * (t_target - t_outdoor)^(1/n)
    //
    // hc (heat curve): 0.5-1.5, maps to building insulation
    // n (exponent): 1.2-1.33 for panel radiators, 1.0 for underfloor heating
    t_flow = t_target + this->shift_ + this->hc_ * powf(delta_t, 1.0f / this->n_);
  } else {
    // No heating needed - outdoor is at or above target temperature
    t_flow = this->min_flow_temp_;
  }

  // Clamp to boiler limits
  return clamp(t_flow, this->min_flow_temp_, this->max_flow_temp_);
}

}  // namespace equitherm
}  // namespace esphome
