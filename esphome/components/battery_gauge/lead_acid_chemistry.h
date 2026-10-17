#pragma once

#include "battery_chemistry.h"

namespace esphome::battery_gauge {

// Lead-acid (flooded, AGM, or a user-tuned "custom" chemistry): SoC-dependent charge acceptance
// that falls off above a knee, Peukert-derated discharge, and a configurable tail-current + dwell
// full-charge test. Absorption voltage is reached well before the battery is actually full, so a
// single instantaneous voltage+tail sample is not a reliable full-charge signal on its own.
class LeadAcidChemistry : public BatteryChemistry {
 public:
  LeadAcidChemistry(float acceptance_knee, float peukert_exponent, float rated_current, float tail_current,
                    uint32_t full_charge_dwell_ms)
      : acceptance_knee_(acceptance_knee),
        peukert_exponent_(peukert_exponent),
        rated_current_(rated_current),
        tail_current_(tail_current),
        full_charge_dwell_ms_(full_charge_dwell_ms) {}

  float charge_acceptance(float soc) const override;
  float discharge_scale(float current) const override;
  bool is_full(const GaugeState &state) const override;
  uint32_t full_charge_dwell_ms() const override { return this->full_charge_dwell_ms_; }

 protected:
  float acceptance_knee_;          // SoC (0..1) above which charge acceptance starts falling off
  float peukert_exponent_;         // k in delta_ah_effective = delta_ah * (I / I_rated)^(k-1); 1.0 = no-op
  float rated_current_;            // I_rated = capacity / capacity_rate, in A
  float tail_current_;             // fraction of capacity below which charging is considered "tapered"
  uint32_t full_charge_dwell_ms_;  // how long the full condition must hold before resyncing
};

}  // namespace esphome::battery_gauge
