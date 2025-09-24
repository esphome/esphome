#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace grove_gas_mc_v2 {

class GroveGasMultichannelV2Component : public PollingComponent, public i2c::I2CDevice {
  SUB_SENSOR(tvoc)
  SUB_SENSOR(carbon_monoxide)
  SUB_SENSOR(nitrogen_dioxide)
  SUB_SENSOR(ethanol)

 public:
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

  bool read_sensor_(uint8_t address, sensor::Sensor *sensor);
};

}  // namespace grove_gas_mc_v2
}  // namespace esphome
