#pragma once

#include <cstdint>
#include <optional>

namespace esphome::battery_gauge {

// Snapshot of gauge state passed to BatteryChemistry::is_full(). Kept as a plain struct so the
// interface can grow without changing every implementation's signature.
struct GaugeState {
  float filtered_voltage;
  float filtered_current;
  float charge_state;  // Ah remaining
  float capacity;      // Ah
  float max_charge_voltage;
};

// Strategy interface for chemistry-specific charge/discharge behaviour. One instance is created
// per battery_gauge sensor and owns whatever chemistry-specific parameters it needs.
class BatteryChemistry {
 public:
  virtual ~BatteryChemistry() = default;

  // Fraction of an incoming charge current that actually adds charge, given the current state of
  // charge (0..1). 1.0 means no derating.
  virtual float charge_acceptance(float soc) const { return 1.0f; }

  // Multiplier applied to a discharge current's magnitude to account for rate-dependent capacity
  // loss (e.g. Peukert's law for lead-acid). 1.0 means no derating.
  virtual float discharge_scale(float current) const { return 1.0f; }

  // Estimate the state of charge (0..1) from a resting open-circuit voltage and temperature, if
  // this chemistry has a usable voltage curve. Returns nullopt if not supported, or the battery
  // is not currently at rest.
  virtual std::optional<float> soc_from_ocv(float voltage, float temperature) const { return std::nullopt; }

  // Whether the gauge should consider the battery fully charged right now.
  virtual bool is_full(const GaugeState &state) const = 0;

  // How long the is_full() condition must hold continuously before the gauge resynchronises to
  // 100%. 0 (the default) resynchronises as soon as the condition is met.
  virtual uint32_t full_charge_dwell_ms() const { return 0; }
};

// LiFePO4 / general lithium-ion: no charge-acceptance or Peukert derating, no OCV curve (the
// voltage plateau makes voltage-based SoC estimation unreliable from ~20-90%), full charge
// detected by voltage + tail-current with no dwell.
class LithiumChemistry : public BatteryChemistry {
 public:
  bool is_full(const GaugeState &state) const override {
    return state.filtered_voltage >= state.max_charge_voltage && state.filtered_current < state.capacity * 0.02f;
  }
};

}  // namespace esphome::battery_gauge
