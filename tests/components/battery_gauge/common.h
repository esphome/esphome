#pragma once
#include <gtest/gtest.h>
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/battery_gauge/sensor/battery_gauge_sensor.h"

namespace esphome::battery_gauge::testing {

// Subclass exposing the protected members of BatteryGaugeSensor for white-box
// testing of the charge-tracking logic.
class TestableBatteryGauge : public BatteryGaugeSensor {
 public:
  using BatteryGaugeSensor::BatteryGaugeSensor;

  using BatteryGaugeSensor::on_current_;
  using BatteryGaugeSensor::on_voltage_;
  using BatteryGaugeSensor::publish_;

  using BatteryGaugeSensor::capacity_;
  using BatteryGaugeSensor::charge_percentage_;
  using BatteryGaugeSensor::charge_state_;
  using BatteryGaugeSensor::efficiency_;
  using BatteryGaugeSensor::filtered_current_;
  using BatteryGaugeSensor::filtered_voltage_;
  using BatteryGaugeSensor::initial_state_;
  using BatteryGaugeSensor::last_current_;
  using BatteryGaugeSensor::last_time_;
  using BatteryGaugeSensor::max_charge_voltage_;
};

// Fixture used by the battery gauge unit tests.

class BatteryGaugeTest : public ::testing::Test {
 protected:
  // Source sensors are owned by the fixture; the gauge holds pointers to them.
  sensor::Sensor voltage_source_;
  sensor::Sensor current_source_;
};

}  // namespace esphome::battery_gauge::testing
