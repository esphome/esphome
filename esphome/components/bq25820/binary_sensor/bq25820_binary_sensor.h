#pragma once

#include "../bq25820.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome::bq25820 {

// Charge cycle status, derived from CHARGE_STAT[2:0] (datasheet Table 7-29).
class BQ25820ChargingBinarySensor : public BQ25820Listener, public binary_sensor::BinarySensor {
 public:
  void on_data(const BQ25820Data &data) override {
    switch (data.charge_stat) {
      case BQ25820Component::CHARGE_STAT_TRICKLE:
      case BQ25820Component::CHARGE_STAT_PRECHARGE:
      case BQ25820Component::CHARGE_STAT_FAST:
      case BQ25820Component::CHARGE_STAT_TAPER:
      case BQ25820Component::CHARGE_STAT_TOP_OFF:
        this->publish_state(true);
        break;
      default:
        this->publish_state(false);
    }
  }
};

class BQ25820ChargedBinarySensor : public BQ25820Listener, public binary_sensor::BinarySensor {
 public:
  void on_data(const BQ25820Data &data) override {
    this->publish_state(data.charge_stat == BQ25820Component::CHARGE_STAT_DONE);
  }
};

class BQ25820LowBatteryBinarySensor : public BQ25820Listener, public binary_sensor::BinarySensor {
 public:
  void on_data(const BQ25820Data &data) override {
    this->publish_state(data.charge_stat == BQ25820Component::CHARGE_STAT_PRECHARGE);
  }
};

// Power path and mode status
class BQ25820PgBinarySensor : public BQ25820Listener, public binary_sensor::BinarySensor {
 public:
  void on_data(const BQ25820Data &data) override { this->publish_state(data.pg); }
};

class BQ25820ReverseBinarySensor : public BQ25820Listener, public binary_sensor::BinarySensor {
 public:
  void on_data(const BQ25820Data &data) override { this->publish_state(data.reverse); }
};

// Fault status
class BQ25820VacUvBinarySensor : public BQ25820Listener, public binary_sensor::BinarySensor {
 public:
  void on_data(const BQ25820Data &data) override { this->publish_state(data.vac_undervoltage); }
};

class BQ25820VacOvBinarySensor : public BQ25820Listener, public binary_sensor::BinarySensor {
 public:
  void on_data(const BQ25820Data &data) override { this->publish_state(data.vac_overvoltage); }
};

class BQ25820IbatOcBinarySensor : public BQ25820Listener, public binary_sensor::BinarySensor {
 public:
  void on_data(const BQ25820Data &data) override { this->publish_state(data.ibat_overcurrent); }
};

class BQ25820VbatOvBinarySensor : public BQ25820Listener, public binary_sensor::BinarySensor {
 public:
  void on_data(const BQ25820Data &data) override { this->publish_state(data.vbat_overvoltage); }
};

class BQ25820ThermalShutdownBinarySensor : public BQ25820Listener, public binary_sensor::BinarySensor {
 public:
  void on_data(const BQ25820Data &data) override { this->publish_state(data.thermal_shutdown); }
};

class BQ25820ChargeSafetyTimerBinarySensor : public BQ25820Listener, public binary_sensor::BinarySensor {
 public:
  void on_data(const BQ25820Data &data) override { this->publish_state(data.charge_safety_timer); }
};

class BQ25820DrvSupplyInvalidBinarySensor : public BQ25820Listener, public binary_sensor::BinarySensor {
 public:
  void on_data(const BQ25820Data &data) override { this->publish_state(data.drv_sup_invalid); }
};

}  // namespace esphome::bq25820
