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

  // Industry-standard European heating curve:
  // t_flow = t_target + shift + hc * (t_target - t_outdoor)^(1/n)
  //
  // hc (heat curve): 0.5-1.5, maps to building insulation
  // n (exponent): 1.2-1.33 for panel radiators, 1.0 for underfloor heating
  float t_flow = t_target + this->shift_;

  // Only apply curve when outdoor is below target (heating needed)
  if (delta_t > 0.0f) {
    t_flow += this->hc_ * powf(delta_t, 1.0f / this->n_);
  }

  // Clamp to boiler limits
  return clamp(t_flow, this->min_flow_temp_, this->max_flow_temp_);
}

}  // namespace equitherm
}  // namespace esphome
