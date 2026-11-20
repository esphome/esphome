#pragma once

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/battery_gauge/battery_chemistry.h"

namespace esphome::battery_gauge {

// Persisted gauge state. This shape must stay stable: resizing it changes the preference key and
// silently discards every user's saved charge on upgrade. Reserved fields exist so later phases
// (last-full timestamp, tuned efficiency, estimated internal resistance) can be added without
// another breaking change.
struct PersistedState {
  uint16_t percentage_x10{0};  // charge percentage * 10 (0..1000)
  uint16_t reserved16{0};
  uint32_t reserved32_a{0};
  uint32_t reserved32_b{0};
};

// Hub component: owns the coulomb-counting state for one physical battery. Individual readings
// (state of charge today; time-to-full, charge phase, etc. later) are exposed by separate
// sensor/binary_sensor/text_sensor platform entries that attach themselves via the set_*_sensor()
// setters below, so a single battery can grow more exposed readings without changing this class's
// public config surface.
class BatteryGauge : public Component {
 public:
  BatteryGauge(sensor::Sensor *voltage_source, sensor::Sensor *current_source, float capacity, float efficiency,
               float max_charge_voltage, BatteryChemistry *chemistry)
      : voltage_source_(voltage_source),
        current_source_(current_source),
        capacity_(capacity),
        efficiency_(efficiency),
        max_charge_voltage_(max_charge_voltage),
        chemistry_(chemistry) {}
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void set_initial_state(float initial_state) { initial_state_ = initial_state; }
  /// Stable, ID-derived key for the persisted charge preference (computed by codegen).
  void set_preference_key(uint32_t key) { this->pref_key_ = key; }
  void set_state_of_charge_sensor(sensor::Sensor *sensor) { this->state_of_charge_sensor_ = sensor; }

 protected:
  void publish_(float new_state);
  // Convert a 0..1 fraction into the persisted percentage-times-ten representation used by publish_().
  static unsigned fraction_to_percentage_x10(float fraction);
  // Convert the persisted percentage-times-ten representation back into a charge state in Ah.
  static float percentage_x10_to_state(unsigned percentage_x10, float capacity);
  // Time-based exponential moving average: alpha = 1 - exp(-dt/tau), so convergence speed
  // doesn't depend on how often samples arrive.
  static float ema_update(float filtered, float raw, float dt_s, float tau_s);
  void on_current_(float value);
  void on_voltage_(float value);

  sensor::Sensor *voltage_source_;
  sensor::Sensor *current_source_;
  float capacity_;
  float efficiency_;
  float max_charge_voltage_;
  BatteryChemistry *chemistry_;

  sensor::Sensor *state_of_charge_sensor_{nullptr};

  float charge_state_{};  // charge state in Ah, remaining
  PersistedState persisted_{};
  float initial_state_{0};
  uint32_t pref_key_{0};

  uint32_t last_time_{0};             // on_current_() sample timestamp, for dt and integration
  uint32_t last_voltage_time_{0};     // on_voltage_() sample timestamp, for dt
  uint32_t last_saved_time_{0};       // last time the preference was actually written
  uint32_t full_condition_since_{0};  // when is_full() first became true, 0 if not currently true

  float filtered_voltage_{NAN};
  float filtered_current_{NAN};
  float last_current_{NAN};
  ESPPreferenceObject saved_state_;
};

}  // namespace esphome::battery_gauge
