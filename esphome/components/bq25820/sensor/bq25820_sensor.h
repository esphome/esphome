#pragma once

#include "../bq25820.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::bq25820 {

// Voltage sensors report in volts; ADC readings are stored in millivolts.
class BQ25820VbatSensor : public BQ25820Listener, public sensor::Sensor {
 public:
  void on_data(const BQ25820Data &data) override { this->publish_state(data.vbat_mv / 1000.0f); }
};

class BQ25820VsysSensor : public BQ25820Listener, public sensor::Sensor {
 public:
  void on_data(const BQ25820Data &data) override { this->publish_state(data.vsys_mv / 1000.0f); }
};

class BQ25820VacSensor : public BQ25820Listener, public sensor::Sensor {
 public:
  void on_data(const BQ25820Data &data) override { this->publish_state(data.vac_mv / 1000.0f); }
};

// TS pin NTC temperature, assuming a 10k NTC to PGND with a 10k reference
// resistor.
class BQ25820TempSensor : public BQ25820Listener, public sensor::Sensor {
 public:
  void on_data(const BQ25820Data &data) override { this->publish_state(data.ts_temp_C); }
};

// Current sensors report in amperes; ADC readings are stored in milliamperes.
// IBAT is signed: positive = charging, negative = discharging.
class BQ25820IbatSensor : public BQ25820Listener, public sensor::Sensor {
 public:
  void on_data(const BQ25820Data &data) override { this->publish_state(data.ibat_ma / 1000.0f); }
};

class BQ25820IacSensor : public BQ25820Listener, public sensor::Sensor {
 public:
  void on_data(const BQ25820Data &data) override { this->publish_state(data.iac_ma / 1000.0f); }
};

}  // namespace esphome::bq25820
