#pragma once

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace grove_gas_mc_v2 {

class GroveGasMultichannelV2Component : public PollingComponent, public i2c::I2CDevice {
 public:
  // Setters
  void set_tvoc(sensor::Sensor *tvoc) { tvoc_ = tvoc; }
  void set_carbon_monoxide(sensor::Sensor *co) { co_ = co; }
  void set_no2(sensor::Sensor *no2) { no2_ = no2; }
  void set_ethanol(sensor::Sensor *ethanol) { ethanol_ = ethanol; }

  /// Setup the sensor and test for a connection.
  void setup() override;
  /// Schedule temperature+pressure readings.
  void update() override;

  void dump_config() override;

  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  enum ErrorCode {
    UNKNOWN,
    COMMUNICATION_FAILED,
    APP_INVALID,
    APP_START_FAILED,
  } error_code_{UNKNOWN};

  // Volatile Organic Compounds Sensor
  sensor::Sensor *tvoc_{nullptr};

  // Carbon Monoxide Sensor
  sensor::Sensor *co_{nullptr};

  // Nitrogen Dioxide Sensor
  sensor::Sensor *no2_{nullptr};

  // Ethanol Sensor
  sensor::Sensor *ethanol_{nullptr};
};

}  // namespace grove_gas_mc_v2
}  // namespace esphome
