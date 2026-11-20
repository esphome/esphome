#include "lead_acid_chemistry.h"
#include <cmath>
#include <algorithm>

namespace esphome::battery_gauge {

float LeadAcidChemistry::charge_acceptance(float soc) const {
  if (this->acceptance_knee_ >= 1.0f || soc <= this->acceptance_knee_)
    return 1.0f;
  float acceptance = (1.0f - soc) / (1.0f - this->acceptance_knee_);
  return std::max(0.0f, std::min(1.0f, acceptance));
}

float LeadAcidChemistry::discharge_scale(float current) const {
  if (this->peukert_exponent_ == 1.0f || this->rated_current_ <= 0.0f)
    return 1.0f;
  float ratio = std::abs(current) / this->rated_current_;
  if (ratio <= 0.0f)
    return 1.0f;
  return std::pow(ratio, this->peukert_exponent_ - 1.0f);
}

bool LeadAcidChemistry::is_full(const GaugeState &state) const {
  return state.filtered_voltage >= state.max_charge_voltage &&
         state.filtered_current < state.capacity * this->tail_current_;
}

}  // namespace esphome::battery_gauge
