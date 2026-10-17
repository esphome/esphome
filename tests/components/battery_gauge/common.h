#pragma once
#include <gtest/gtest.h>
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/battery_gauge/battery_gauge.h"
#include "esphome/components/battery_gauge/lead_acid_chemistry.h"

namespace esphome::battery_gauge::testing {

// Subclass exposing the protected members of BatteryGauge for white-box testing of the
// charge-tracking logic. Automatically wires a state-of-charge sensor (soc_sensor_) so tests can
// assert on published state the same way the real sensor platform would see it.
class TestableBatteryGauge : public BatteryGauge {
 public:
  TestableBatteryGauge(sensor::Sensor *voltage_source, sensor::Sensor *current_source, float capacity, float efficiency,
                       float max_charge_voltage, BatteryChemistry *chemistry)
      : BatteryGauge(voltage_source, current_source, capacity, efficiency, max_charge_voltage, chemistry) {
    this->set_state_of_charge_sensor(&this->soc_sensor_);
  }

  using BatteryGauge::on_current_;
  using BatteryGauge::on_voltage_;
  using BatteryGauge::publish_;
  using BatteryGauge::fraction_to_percentage_x10;
  using BatteryGauge::percentage_x10_to_state;
  using BatteryGauge::ema_update;

  using BatteryGauge::capacity_;
  using BatteryGauge::chemistry_;
  using BatteryGauge::charge_state_;
  using BatteryGauge::efficiency_;
  using BatteryGauge::filtered_current_;
  using BatteryGauge::filtered_voltage_;
  using BatteryGauge::full_condition_since_;
  using BatteryGauge::initial_state_;
  using BatteryGauge::last_current_;
  using BatteryGauge::last_saved_time_;
  using BatteryGauge::last_time_;
  using BatteryGauge::last_voltage_time_;
  using BatteryGauge::max_charge_voltage_;
  using BatteryGauge::persisted_;

  sensor::Sensor soc_sensor_;
};

// Fixture used by the battery gauge unit tests.
class BatteryGaugeTest : public ::testing::Test {
 protected:
  // Source sensors are owned by the fixture; the gauge holds pointers to them.
  sensor::Sensor voltage_source_;
  sensor::Sensor current_source_;
  // Stateless default chemistry, reused by every test that doesn't care about chemistry-specific
  // behaviour (that is, everything except the lead-acid tests).
  LithiumChemistry lithium_chemistry_;
};

}  // namespace esphome::battery_gauge::testing
