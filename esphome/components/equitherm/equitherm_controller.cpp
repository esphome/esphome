#include "equitherm_controller.h"
#include "esphome/core/helpers.h"

namespace esphome::equitherm {

float HeatingCurve::compute_flow_temperature(float t_target, float t_outdoor) {
  // Guard against invalid inputs
  if (std::isnan(t_target) || std::isnan(t_outdoor)) {
    return min_flow_temp_;
  }

  float delta_t = t_target - t_outdoor;

  // Warm weather shutdown: outdoor >= target — bypass formula entirely.
  // Return 0 to signal no heating demand.
  // Shift is only valid within the heating domain (delta_t > 0); applying it
  // at delta_t <= 0 would defeat warm weather shutdown and supply heat
  // unnecessarily. See: Viessmann, Buderus, EN 12831, Versatile Thermostat.
  if (delta_t <= 0.0f) {
    return 0.0f;
  }

  // Industry-standard European heating curve:
  // t_flow = t_target + shift + hc * (t_target - t_outdoor)^(1/n)
  float t_flow = t_target + this->shift_ + this->hc_ * powf(delta_t, 1.0f / this->n_);

  // Clamp to boiler limits
  return clamp(t_flow, this->min_flow_temp_, this->max_flow_temp_);
}

}  // namespace esphome::equitherm
