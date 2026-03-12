#include "equitherm_controller.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace equitherm_climate {

float EquithermController::compute(float t_target, float t_outdoor) {
  // Guard against invalid inputs
  if (std::isnan(t_target) || std::isnan(t_outdoor)) {
    return t_min_flow_;
  }

  // Guard against mathematical singularity when t_max_flow == t_target
  // In this case, the formula produces 0/0 = NaN
  if (fabsf(t_max_flow_ - t_target) < 0.1f) {
    return t_min_flow_;
  }

  float temp_delta = t_target - t_outdoor;

  // maxPoint = outdoor temp at which curve approaches t_max_flow
  // From OTGateway: maxPoint = t_target - (t_max_flow - t_target) / slope
  float max_point = t_target - (t_max_flow_ - t_target) / slope_;

  // Scale factor -- normalizes curve to pass through (maxPoint, t_max_flow)
  float sf = (t_max_flow_ - t_target) / powf(t_target - max_point, 1.0f / exponent_);

  // Guard against nan from scale factor calculation
  if (std::isnan(sf)) {
    return t_min_flow_;
  }

  // Curve -- handles both positive and negative delta
  // For positive delta (outdoor < target): use pow(temp_delta, 1/exp)
  // For negative delta (outdoor > target): use -pow(-temp_delta, 1/exp)
  float t_flow = t_target + shift_ +
                 sf * (temp_delta >= 0.0f ? powf(temp_delta, 1.0f / exponent_) : -powf(-temp_delta, 1.0f / exponent_));

  // Clamp to boiler limits
  return clamp(t_flow, t_min_flow_, t_max_flow_);
}

}  // namespace equitherm_climate
}  // namespace esphome
